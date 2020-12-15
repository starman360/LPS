#ifndef TRIANGULATION_H
#define TRIANGULATION_H
#include <math.h>
#include "esp_system.h"

double dist[4][4]={
    { 0.0, 0.0, 0.0, 0.0},
    { 0.0, 0.0, 0.0, 0.0},
    { 0.0, 0.0, 0.0, 0.0},
    { 0.0, 0.0, 0.0, 0.0},
};

typedef struct {
    uint16_t id;
    double x;
    double y;
    double z;
    uint8_t qf;
} coords;

typedef struct {
    uint8_t size;
    coords n1;
    coords n2;
    coords n3;
    coords n4;
}global_xyz;

global_xyz xyz = {0, }; // 4, {0,0.0,0.0,0.0,0.0}, {0,0.0,0.0,0.0,0.0}, {0,0.0,0.0,0.0,0.0}, {0,0.0,0.0,0.0,0.0}};
uint16_t nodeorder[4] = {0,0,0,0};

/* Function which takes in the global distance array and updates the node positions */
void get_coords(){
    uint8_t size = xyz.size;

    if(size == 0) return;
    if(size > 0){
        /* set initiator/master to origin */
        xyz.n1.x = 0;
        xyz.n1.y = 0;
        xyz.n1.z = 0;
    }
    if(size > 1){
        /* set second node to x-axis of origin */
        xyz.n2.x = dist[1][0];
        xyz.n2.y = 0;
        xyz.n2.z = 0;
    }
    if(size > 2){
        /* set third node to same z-plane as first and second node */
        xyz.n3.x = (dist[2][1]*dist[2][1] - dist[2][0]*dist[2][0] - dist[1][0]*dist[1][0])/(-2*dist[1][0]*dist[1][0]);
        xyz.n3.y = sqrt(dist[2][0]*dist[2][0]- xyz.n3.x*xyz.n3.x);
        xyz.n3.z = 0;
    }
    if(size > 3){
        /* get fourth node coords */
        xyz.n4.x = (dist[3][1]*dist[3][1] - dist[3][0]*dist[3][0] - dist[1][0]*dist[1][0])/(-2*(dist[1][0])); 
        xyz.n4.y = (dist[3][2]*dist[3][2] - dist[3][0]*dist[3][0] 
                    - ((xyz.n4.x - xyz.n3.x)*(xyz.n4.x - xyz.n3.x) - xyz.n4.x*xyz.n4.x) 
                    - xyz.n3.y*xyz.n3.y)/(-2*xyz.n3.y);
        xyz.n4.z = sqrt(dist[3][0]*dist[3][0] - xyz.n4.x*xyz.n4.x - xyz.n4.y*xyz.n4.y);
    }
    return;    
}

// Gets the index of the id or the next available one. 
uint8_t getnodeorder(uint16_t id){
    printf("Getting node id for : %d\n", id);
    for (uint8_t i = 0; i < 4; i++){
        if (nodeorder[i] == id){
            return i;
        }
    }
    for (uint8_t i = 0; i < 4; i++){
        if (nodeorder[i] == 0){
            nodeorder[i] = id;
            xyz.size = i+1;
            return i;
        }
    }
    return -1;
}

void setDistance(uint8_t fromloc, uint16_t toID, uint32_t distance){
    if (toID == 17041 || toID == 17050 || toID == 52148 || toID == 16899 || toID == 23440 || toID == 19592 || toID == 21433){
        uint8_t toloc = getnodeorder(toID);
        dist[fromloc][toloc] = distance;
    }
    printf("Got node id\n");
}

#endif