#include <iostream>
#include <math.h>
#include <random>
#include <vector>

#include "cloth.h"
#include "collision/plane.h"
#include "collision/sphere.h"

using namespace std;

Cloth::Cloth(double width, double height, int num_width_points,
             int num_height_points, int num_length_points, float thickness) {
  this->width = width;
  this->height = height;
  this->num_width_points = num_width_points;
  this->num_height_points = num_height_points;
  this->num_length_points = num_length_points;
  this->thickness = thickness;

  buildGrid();
  buildClothMesh();
}

Cloth::~Cloth() {
  point_masses.clear();
  springs.clear();

  if (clothMesh) {
    delete clothMesh;
  }
}

void Cloth::buildGrid() {
  // TODO (Part 1): Build a grid of masses and springs.

    for (int col = 0; col < num_height_points; col++) {
        for (int row = 0; row < num_width_points; row++) {
            for (int len = 0; len < num_length_points; len++) {
                double x = width * row / (double(num_width_points) - 1.0);
                double y = height * col / (double(num_height_points) - 1.0);
                
                // may need to add more stuff to take the 3D into consideration
                vector<int> xy{ row, col };

                Vector3D pos;
                bool pin;
                
                if (orientation == HORIZONTAL) {
                    pos = Vector3D(x, 1.0, y);
                }
                else {
                    pos = Vector3D(x, y, (rand() % 2 - 1) / 1000.0);
                }

                if (std::find(pinned.begin(), pinned.end(), xy) != pinned.end()) {
                    pin = true;
                }
                else {
                    pin = false;
                }

                point_masses.emplace_back(PointMass(pos, pin));
            }
        }
    }

    for (int row = 0; row < num_width_points; row++) {
        for (int col = 0; col < num_height_points; col++) {
            for (int len = 0; len < num_length_points; len++) {
                if (row - 1 >= 0) {
                    springs.emplace_back(Spring(&point_masses[col * num_width_points + row], &point_masses[col * num_width_points + row - 1], STRUCTURAL));
                }
                if (col - 1 >= 0) {
                    springs.emplace_back(Spring(&point_masses[col * num_width_points + row], &point_masses[(col - 1) * num_width_points + row], STRUCTURAL));
                }
                if (row - 1 >= 0 && col - 1 >= 0) {
                    springs.emplace_back(Spring(&point_masses[col * num_width_points + row], &point_masses[(col - 1) * num_width_points + row - 1], SHEARING));
                }
                if (row + 1 < num_width_points && col - 1 >= 0) {
                    springs.emplace_back(Spring(&point_masses[col * num_width_points + row], &point_masses[(col - 1) * num_width_points + row + 1], SHEARING));
                }
                if (row - 2 >= 0) {
                    springs.emplace_back(Spring(&point_masses[col * num_width_points + row], &point_masses[col * num_width_points + row - 2], BENDING));
                }
                if (col - 2 >= 0) {
                    springs.emplace_back(Spring(&point_masses[col * num_width_points + row], &point_masses[(col - 2) * num_width_points + row], BENDING));
                }
            }
        }
    }

}

void Cloth::simulate(double frames_per_sec, double simulation_steps, ClothParameters *cp,
                     vector<Vector3D> external_accelerations,
                     vector<CollisionObject *> *collision_objects) {
  double mass = width * height * cp->density / num_width_points / num_height_points;
  double delta_t = 1.0f / frames_per_sec / simulation_steps;

  // TODO (Part 2): Compute total force acting on each point mass.

  for (PointMass& pm : this->point_masses) {
      pm.forces = Vector3D(0.0, 0.0, 0.0);

      for (auto ext : external_accelerations) {
          pm.forces += ext * mass;
      }
  }

  for (Spring sp : springs) {
      switch (sp.spring_type) {
      case STRUCTURAL:
          if (cp->enable_structural_constraints) {
              double Fs = cp->ks * ((sp.pm_a->position - sp.pm_b->position).norm() - sp.rest_length);
              sp.pm_a->forces += Fs * (sp.pm_b->position - sp.pm_a->position).unit();
              sp.pm_b->forces += Fs * (sp.pm_a->position - sp.pm_b->position).unit();
          }
          break;
      case SHEARING:
          if (cp->enable_shearing_constraints) {
              double Fs = cp->ks * ((sp.pm_a->position - sp.pm_b->position).norm() - sp.rest_length);
              sp.pm_a->forces += Fs * (sp.pm_b->position - sp.pm_a->position).unit();
              sp.pm_b->forces += Fs * (sp.pm_a->position - sp.pm_b->position).unit();
          }
          break;
      case BENDING:
          if (cp->enable_bending_constraints) {
              double Fs = cp->ks * 0.2 * ((sp.pm_a->position - sp.pm_b->position).norm() - sp.rest_length);
              sp.pm_a->forces += Fs * (sp.pm_b->position - sp.pm_a->position).unit();
              sp.pm_b->forces += Fs * (sp.pm_a->position - sp.pm_b->position).unit();
          }
          break;
      }
  }

  // TODO (Part 2): Use Verlet integration to compute new point mass positions

  for (PointMass& pm : this->point_masses) {
      if (!pm.pinned) {
          Vector3D newpos = pm.position + (1 - cp->damping / 100.0) * (pm.position - pm.last_position) + pm.forces / mass * delta_t * delta_t;

          pm.last_position = pm.position;
          pm.position = newpos;
      }
  }


  // TODO (Part 4): Handle self-collisions.
  build_spatial_map();

  for (PointMass& pm : this->point_masses) {
      self_collide(pm, simulation_steps);
  }

  // TODO (Part 3): Handle collisions with other primitives.
  for (PointMass& pm : this->point_masses) {
      for (CollisionObject* obj : *collision_objects) {
          obj->collide(pm);
      }
  }


  // TODO (Part 2): Constrain the changes to be such that the spring does not change
  // in length more than 10% per timestep [Provot 1995].

  for (Spring& sp : this->springs) {
      if ((sp.pm_a->position - sp.pm_b->position).norm() > 1.1 * sp.rest_length) {
          if (sp.pm_a->pinned && !sp.pm_b->pinned) {
              sp.pm_b->position = sp.pm_a->position + (sp.pm_b->position - sp.pm_a->position).unit() * 1.1 * sp.rest_length;
          }
          else if (!sp.pm_a->pinned && sp.pm_b->pinned) {
              sp.pm_a->position = sp.pm_b->position + (sp.pm_a->position - sp.pm_b->position).unit() * 1.1 * sp.rest_length;
          }
          else if (!sp.pm_a->pinned && !sp.pm_b->pinned) {
              double correction = (sp.pm_a->position - sp.pm_b->position).norm() - 1.1 * sp.rest_length;
              sp.pm_a->position = sp.pm_a->position + correction / 2.0 * (sp.pm_b->position - sp.pm_a->position).unit();
              sp.pm_b->position = sp.pm_b->position + correction / 2.0 * (sp.pm_a->position - sp.pm_b->position).unit();
          }
      }
  }

}

void Cloth::build_spatial_map() {
  for (const auto &entry : map) {
    delete(entry.second);
  }
  map.clear();

  // TODO (Part 4): Build a spatial map out of all of the point masses.
  for (PointMass& pm : this->point_masses) {
      float slot = hash_position(pm.position);
      //new bucket
      if (map.count(slot) == 0) {
          vector<PointMass*> *bucket = new vector<PointMass*>();
          bucket->emplace_back(&pm);
          map[slot] = bucket;
      }
      //old bucket
      else {
          map[slot]->emplace_back(&pm);
      }
  }
  
}

void Cloth::self_collide(PointMass &pm, double simulation_steps) {
  // TODO (Part 4): Handle self-collision for a given point mass.
    Vector3D pos = pm.position;
    vector<PointMass*> bucket = *map.at(hash_position(pos));

    Vector3D avg(0.0);
    double numCorr = 0.0;
    for (PointMass* pmass : bucket) {
        if (pmass != &pm) {
            Vector3D vec = pm.position - pmass->position;
            double dist = vec.norm();
            //cout << dist << endl;
            if (dist < 2 * thickness) {
                //compute correction vector
                //cout << "zz" << endl;
                avg += vec.unit() * (2 * thickness - dist);
                numCorr += 1;
            }
        }
    }
    if (numCorr > 0) {
        avg *= 1.0 / numCorr / simulation_steps;
        pm.position += avg;
    }
}

float Cloth::hash_position(Vector3D pos) {
  // TODO (Part 4): Hash a 3D position into a unique float identifier that represents membership in some 3D box volume.
    //return 1.0f;
    float w = 3 * width / num_width_points;
    float h = 3 * height / num_height_points;
    float t = max(w, h);
    float x = floor(pos.x/w);
    float y = floor(pos.y/h);
    float z = floor(pos.z/t);
    float val = x + (y * width) + (z * width * height);

  return val; 
}

///////////////////////////////////////////////////////
/// YOU DO NOT NEED TO REFER TO ANY CODE BELOW THIS ///
///////////////////////////////////////////////////////

void Cloth::reset() {
  PointMass *pm = &point_masses[0];
  for (int i = 0; i < point_masses.size(); i++) {
    pm->position = pm->start_position;
    pm->last_position = pm->start_position;
    pm++;
  }
}

void Cloth::buildClothMesh() {
  if (point_masses.size() == 0) return;

  ClothMesh *clothMesh = new ClothMesh();
  vector<Triangle *> triangles;

  // Create vector of triangles
  for (int y = 0; y < num_height_points - 1; y++) {
    for (int x = 0; x < num_width_points - 1; x++) {
      PointMass *pm = &point_masses[y * num_width_points + x];
      // Get neighboring point masses:
      /*                      *
       * pm_A -------- pm_B   *
       *             /        *
       *  |         /   |     *
       *  |        /    |     *
       *  |       /     |     *
       *  |      /      |     *
       *  |     /       |     *
       *  |    /        |     *
       *      /               *
       * pm_C -------- pm_D   *
       *                      *
       */
      
      float u_min = x;
      u_min /= num_width_points - 1;
      float u_max = x + 1;
      u_max /= num_width_points - 1;
      float v_min = y;
      v_min /= num_height_points - 1;
      float v_max = y + 1;
      v_max /= num_height_points - 1;
      
      PointMass *pm_A = pm                       ;
      PointMass *pm_B = pm                    + 1;
      PointMass *pm_C = pm + num_width_points    ;
      PointMass *pm_D = pm + num_width_points + 1;
      
      Vector3D uv_A = Vector3D(u_min, v_min, 0);
      Vector3D uv_B = Vector3D(u_max, v_min, 0);
      Vector3D uv_C = Vector3D(u_min, v_max, 0);
      Vector3D uv_D = Vector3D(u_max, v_max, 0);
      
      
      // Both triangles defined by vertices in counter-clockwise orientation
      triangles.push_back(new Triangle(pm_A, pm_C, pm_B, 
                                       uv_A, uv_C, uv_B));
      triangles.push_back(new Triangle(pm_B, pm_C, pm_D, 
                                       uv_B, uv_C, uv_D));
    }
  }

  // For each triangle in row-order, create 3 edges and 3 internal halfedges
  for (int i = 0; i < triangles.size(); i++) {
    Triangle *t = triangles[i];

    // Allocate new halfedges on heap
    Halfedge *h1 = new Halfedge();
    Halfedge *h2 = new Halfedge();
    Halfedge *h3 = new Halfedge();

    // Allocate new edges on heap
    Edge *e1 = new Edge();
    Edge *e2 = new Edge();
    Edge *e3 = new Edge();

    // Assign a halfedge pointer to the triangle
    t->halfedge = h1;

    // Assign halfedge pointers to point masses
    t->pm1->halfedge = h1;
    t->pm2->halfedge = h2;
    t->pm3->halfedge = h3;

    // Update all halfedge pointers
    h1->edge = e1;
    h1->next = h2;
    h1->pm = t->pm1;
    h1->triangle = t;

    h2->edge = e2;
    h2->next = h3;
    h2->pm = t->pm2;
    h2->triangle = t;

    h3->edge = e3;
    h3->next = h1;
    h3->pm = t->pm3;
    h3->triangle = t;
  }

  // Go back through the cloth mesh and link triangles together using halfedge
  // twin pointers

  // Convenient variables for math
  int num_height_tris = (num_height_points - 1) * 2;
  int num_width_tris = (num_width_points - 1) * 2;

  bool topLeft = true;
  for (int i = 0; i < triangles.size(); i++) {
    Triangle *t = triangles[i];

    if (topLeft) {
      // Get left triangle, if it exists
      if (i % num_width_tris != 0) { // Not a left-most triangle
        Triangle *temp = triangles[i - 1];
        t->pm1->halfedge->twin = temp->pm3->halfedge;
      } else {
        t->pm1->halfedge->twin = nullptr;
      }

      // Get triangle above, if it exists
      if (i >= num_width_tris) { // Not a top-most triangle
        Triangle *temp = triangles[i - num_width_tris + 1];
        t->pm3->halfedge->twin = temp->pm2->halfedge;
      } else {
        t->pm3->halfedge->twin = nullptr;
      }

      // Get triangle to bottom right; guaranteed to exist
      Triangle *temp = triangles[i + 1];
      t->pm2->halfedge->twin = temp->pm1->halfedge;
    } else {
      // Get right triangle, if it exists
      if (i % num_width_tris != num_width_tris - 1) { // Not a right-most triangle
        Triangle *temp = triangles[i + 1];
        t->pm3->halfedge->twin = temp->pm1->halfedge;
      } else {
        t->pm3->halfedge->twin = nullptr;
      }

      // Get triangle below, if it exists
      if (i + num_width_tris - 1 < 1.0f * num_width_tris * num_height_tris / 2.0f) { // Not a bottom-most triangle
        Triangle *temp = triangles[i + num_width_tris - 1];
        t->pm2->halfedge->twin = temp->pm3->halfedge;
      } else {
        t->pm2->halfedge->twin = nullptr;
      }

      // Get triangle to top left; guaranteed to exist
      Triangle *temp = triangles[i - 1];
      t->pm1->halfedge->twin = temp->pm2->halfedge;
    }

    topLeft = !topLeft;
  }

  clothMesh->triangles = triangles;
  this->clothMesh = clothMesh;
}
