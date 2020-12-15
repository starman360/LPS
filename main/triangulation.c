#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

double dist_arr[4][4]={
    {    0, 1.414, 2.828, 2.236},
    { 1.414,    0, 2.449, 2.236},
    { 2.828, 2.449,    0, 2.000},
    { 2.236, 2.236, 2.000,    0},
};

typedef struct {
    double x;
    double y;
    double z;
    uint8_t qf;
} coords;

/* Function which takes in the global distance array and updates the node positions */
typedef struct {
    uint8_t size;
    coords n1;
    coords n2;
    coords n3;
    coords n4;
}global_xyz;

void get_coords(global_xyz *xyz, double dist[4][4]){
    uint8_t size = xyz->size;

    if(size == 0) return;
    if(size > 0){
        /* set initiator/master to origin */
        xyz->n1.x = 0;
        xyz->n1.y = 0;
        xyz->n1.z = 0;
    }
    if(size > 1){
        /* set second node to x-axis of origin */
        xyz->n2.x = dist[0][1];
        xyz->n2.y = 0;
        xyz->n2.z = 0;
    }
    if(size > 2){
        /* set third node to same z-plane as first and second node */
        xyz->n3.x = (dist[1][2]*dist[1][2] - dist[0][2]*dist[0][2] - dist[0][1]*dist[0][1])/(-2*dist[0][1]*dist[0][1]);
        xyz->n3.y = sqrt(dist[0][2]*dist[0][2]- xyz->n3.x*xyz->n3.x);
        xyz->n3.z = 0;
    }
    if(size > 3){
        /* get fourth node coords */
        xyz->n4.x = (dist[1][3]*dist[1][3] - dist[0][3]*dist[0][3] - dist[0][1]*dist[0][1])/(-2*(dist[0][1])); 
        xyz->n4.y = (dist[2][3]*dist[2][3] - dist[0][3]*dist[0][3] 
                    - ((xyz->n4.x - xyz->n3.x)*(xyz->n4.x - xyz->n3.x) - xyz->n4.x*xyz->n4.x) 
                    - xyz->n3.y*xyz->n3.y)/(-2*xyz->n3.y);
        xyz->n4.z = sqrt(dist[0][3]*dist[0][3] - xyz->n4.x*xyz->n4.x - xyz->n4.y*xyz->n4.y);
    }
    return;    
}

global_xyz xyz = {4, {0.0,0.0,0.0,0.0}, {0.0,0.0,0.0,0.0}, {0.0,0.0,0.0,0.0}, {0.0,0.0,0.0,0.0}};

void app_main(){
    while(1){
        vTaskDelay(2000/ portTICK_RATE_MS);
        get_coords(&xyz, dist_arr);
        printf("---------- Post Calculation results ----------\n");
        printf("Node \t X \t\t Y \t\t Z\n");
        printf("1 \t %f \t %f \t %f\n", xyz.n1.x, xyz.n1.y, xyz.n1.z);
        printf("2 \t %f \t %f \t %f\n", xyz.n2.x, xyz.n2.y, xyz.n1.z);
        printf("3 \t %f \t %f \t %f\n", xyz.n3.x, xyz.n3.y, xyz.n1.z);
        printf("4 \t %f \t %f \t %f\n", xyz.n4.x, xyz.n4.y, xyz.n4.z);
        vTaskDelay(2000/ portTICK_RATE_MS);
    }
}