#include <iostream>
#include <math.h>
#include <random>
#include <vector>

#include "cloth.h"
#include "collision/plane.h"
#include "collision/sphere.h"

using namespace std;

Cloth::Cloth(double width, double height, int num_width_points,
             int num_height_points, float thickness) {
    this->width = width;
    this->height = height;
    this->num_width_points = num_width_points;
    this->num_height_points = num_height_points;
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
    double wNormal = width / num_width_points;
    double hNormal = height / num_height_points;
    Vector3D spot;
    for (int h = 0; h < num_height_points; h ++){
        for (int w = 0; w < num_width_points; w ++){
            if (orientation == HORIZONTAL){
                //
                spot = Vector3D(w * wNormal, 1.0, h * hNormal);
            } else {
                //
                double z = (rand() / (double) RAND_MAX) * 0.002 - 0.001;
                spot = Vector3D(w * wNormal, h * hNormal, z);
            }
            bool pin = false;
            std::vector<int> index = {w,h};
            if (std::find(pinned.begin(), pinned.end(), index) != pinned.end()){
                pin = true;
            }
            PointMass mas = PointMass(spot, pin);
            point_masses.emplace_back(mas);
        }
    }
    //Springs
    for (int h = 0; h < num_height_points; h ++){
        for (int w = 0; w < num_width_points; w ++){
            if (w > 0) {
                Spring s = Spring(&point_masses[h * num_width_points + w - 1], &point_masses[h * num_width_points + w], STRUCTURAL);
                springs.emplace_back(s);
            }
            if (h > 0) {
                Spring s = Spring(&point_masses[(h - 1) * num_width_points + w], &point_masses[h * num_width_points + w], STRUCTURAL);
                springs.emplace_back(s);
            }
            
            if (h > 0 && w > 0) {
                Spring s = Spring(&point_masses[(h - 1) * num_width_points + w - 1], &point_masses[h * num_width_points + w], SHEARING);
                springs.emplace_back(s);
            }
            if (h > 0 && w < (num_width_points - 1)) { // Diagonal Upper Right Shearing Spring
                Spring s = Spring(&point_masses[h * num_width_points + w], &point_masses[(h - 1) * num_width_points + w + 1], SHEARING);
                springs.emplace_back(s);
            }
            //bending
            if (w > 1) {
                Spring s = Spring(&point_masses[h * num_width_points + w], &point_masses[h * num_width_points + w - 2], BENDING);
                springs.emplace_back(s);
            }
            if (h > 1) {
                Spring s = Spring(&point_masses[h * num_width_points + w], &point_masses[(h - 2) * num_width_points + w], BENDING);
                springs.emplace_back(s);
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
    Vector3D newton = Vector3D(0,0,0);
    for (Vector3D &v : external_accelerations) {
        newton += v;
    }
    Vector3D ext = newton * mass;
    for (PointMass &p : point_masses) {
        p.forces = ext;
    }
    for (Spring &s : springs){
        if((s.spring_type == STRUCTURAL && !cp->enable_structural_constraints) || (s.spring_type == SHEARING && !cp->enable_shearing_constraints) ||(s.spring_type == BENDING && !cp->enable_bending_constraints)){
            continue;
        }
        double Fs = cp -> ks * ((s.pm_a->position - s.pm_b->position).norm() - s.rest_length);
        if (s.spring_type == BENDING) {
            Fs *= 0.2;
        }
        Vector3D Fsv = Fs * ((s.pm_b->position - s.pm_a->position).unit());
        s.pm_a->forces += Fsv;
        s.pm_b->forces -= Fsv;
    }
    
    
    // TODO (Part 2): Use Verlet integration to compute new point mass positions
    for (PointMass &pm : point_masses) {
        if (!pm.pinned) {
            Vector3D accel = pm.forces / mass;
            Vector3D new_pos = pm.position + (1.0 - cp->damping / 100.0) * (pm.position - pm.last_position) + accel * pow(delta_t, 2);
            pm.last_position = pm.position;
            pm.position = new_pos;
            
        }
    }
    
    // TODO (Part 4): Handle self-collisions.
     build_spatial_map();
    for (PointMass &pm : point_masses) {
        self_collide(pm, simulation_steps);
    }
    
    // TODO (Part 3): Handle collisions with other primitives.
    
    for (PointMass &pm : point_masses) {
        for (CollisionObject *c : *collision_objects) {
            c -> collide(pm);
        }
    }
    // TODO (Part 2): Constrain the changes to be such that the spring does not change
    // in length more than 10% per timestep [Provot 1995].

    for (Spring &s : springs){
        Vector3D dir = (s.pm_b->position - s.pm_a->position).unit();
        double length = (s.pm_a->position - s.pm_b->position).norm();
        double limit = s.rest_length * 1.1;
        
        if(length > limit){
            if (!s.pm_a->pinned && !s.pm_b->pinned) {
                s.pm_a->position += ((length - limit) / 2.0) * dir;
                s.pm_b->position -= ((length - limit) / 2.0) * dir;
            } else if (!s.pm_a->pinned) {
                s.pm_a->position += (length - limit) * dir;
            } else if (!s.pm_b->pinned) {
                s.pm_b->position -= (length - limit) * dir;
            } else {
                continue;
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
    for(PointMass &pm : point_masses){
        double hash = hash_position(pm.position);
        if(!map.count(hash)){
            map[hash] = new std::vector<PointMass *>();
        }
        map[hash]->push_back(&pm);
    }
}

void Cloth::self_collide(PointMass &pm, double simulation_steps) {
    // TODO (Part 4): Handle self-collision for a given point mass.
    //For each pair between the point mass and a candidate point mass, determine whether they are within 2 * thickness2∗thickness distance apart.
    //If so, compute a correction vector that can be applied to the point mass (not the candidate one) such that the pair would be 2 * thickness2∗thickness distance apart. The final correction vector to the point mass's position is the average of all of these pairwise correction vectors, scaled down by simulation_steps (this helps improve accuracy by reducing the potential number of sudden position corrections). Make sure to not collide a point mass with itself!
    
    
    float hash = hash_position(pm.position);
    Vector3D correction = Vector3D(0,0,0);
    double count = 0;
    if (map.count(hash)) {
        for (PointMass *p : *map[hash]) {
            if (p != &pm){
                if ((p->position - pm.position).norm() < 2 * thickness){
                correction += (pm.position - p->position).unit() * (2 * thickness - (p->position - pm.position).norm());
                count += 1;
                }
            }
        }
    }
    if (count != 0) {
        pm.position += correction/count/simulation_steps;
    }
}

float Cloth::hash_position(Vector3D pos) {
    // TODO (Part 4): Hash a 3D position into a unique float identifier that represents membership in some 3D box volume.
    double w = 3 * width / num_width_points;
    double h = 3 * height / num_height_points;
    double t = max(w,h);
    
    float x = (pos.x - fmod(pos.x, w)) / w;
    float y = (pos.y - fmod(pos.y, h)) / h;
    float z = (pos.z - fmod(pos.z, t)) / t;
    Vector3D box (x, y, z);
    return box.x + box.y * num_width_points + box.z * num_height_points * num_width_points;
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
