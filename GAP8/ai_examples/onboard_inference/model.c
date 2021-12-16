/*
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */


#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s
/* PMSIS includes. */
#include "pmsis.h"

/* Autotiler includes. */
#include "model.h"
#include "modelKernels.h"
#include "gaplib/ImgIO.h"

//include periph
#include "bsp/ram.h"
#include "bsp/ram/hyperram.h"
#include "bsp/bsp.h"
#include "bsp/camera.h"
#include "bsp/camera/himax.h"
#include "img_proc.h"

#define pmsis_exit(n) exit(n)

#ifndef STACK_SIZE
#define STACK_SIZE      1024
#endif

#if defined(USE_HYPER)
   AT_HYPERFLASH_FS_EXT_ADDR_TYPE model_L3_Flash = 0;
#elif defined(USE_SPI)
   AT_QSPIFLASH_FS_EXT_ADDR_TYPE model_L3_Flash = 0;
#endif

// Softmax always outputs Q15 short int even from 8 bit input
L2_MEM short int *ResOut;
//Image in is unsigned but the model is trained with -1:1 inputs
//The preprocessing to scale the image is done in the CNN AT graph
static uint32_t l3_addr;
uint8_t *imgBuffL2_TEMP;

#define AT_INPUT_WIDTH  28
#define AT_INPUT_HEIGHT 28
#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

#define CAMERA_WIDTH    324
#define CAMERA_HEIGHT   324

//#define PRINT_IMAGE


#define NUM_CLASSES     12

static void cluster()
{
  printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  modelCNN(imgBuffL2_TEMP, ResOut);
  printf("Runner completed\n");

  //Checki Results
  int rec_digit = 0;
  short int highest = ResOut[0];
  for(int i = 0; i < NUM_CLASSES; i++) {
    printf("class %d: %d \n", i, ResOut[i]);
    if(ResOut[i] > highest) {
      highest = ResOut[i];
      rec_digit = i;
    }
  }
  printf("\n");

  printf("Recognized: %d\n", rec_digit);
}


static int32_t open_camera_himax(struct pi_device *device)
{
    struct pi_himax_conf cam_conf;
    pi_himax_conf_init(&cam_conf);

#ifdef SLICE_MODE
    cam_conf.roi.slice_en = 1;
    cam_conf.roi.x = X;
    cam_conf.roi.y = Y;
    cam_conf.roi.w = CAMERA_WIDTH;
    cam_conf.roi.h = CAMERA_HEIGHT;
#endif

    pi_open_from_conf(device, &cam_conf);
    if (pi_camera_open(device))
    {
        return -1;
    }

    // Rotate camera orientation
    pi_camera_control(device, PI_CAMERA_CMD_START, 0);
    uint8_t set_value = 3;
    uint8_t reg_value;

    pi_camera_reg_set(device, IMG_ORIENTATION, &set_value);
    pi_time_wait_us(100000);//0.1s
    pi_camera_reg_get(device, IMG_ORIENTATION, &reg_value);
    if (set_value!=reg_value)
    {
        printf("Failed to rotate camera image\n");
        return -2;
    }
    pi_camera_control(device, PI_CAMERA_CMD_STOP, 0);

    pi_camera_control(device, PI_CAMERA_CMD_AEG_INIT, 0);

    return 0;
}


int inference(void)
{
    printf("Entering main controller\n");
       
    struct pi_device cam;
    struct pi_device ram;
    struct pi_hyperram_conf ram_conf;
    uint8_t *imgBuffL2;
    uint8_t *imgBuffL2_COLOR;
    int32_t errors = 0;

    printf("Open Hyperram\n");
    pi_hyperram_conf_init(&ram_conf);
    pi_open_from_conf(&ram, &ram_conf);
    if (pi_ram_open(&ram))
    {
        printf("Error ram open !\n");
        pmsis_exit(-1);
    }

    printf("allocate buffer for L2\n");
    //alloc for camera buffer
    imgBuffL2 = (uint8_t *) pmsis_l2_malloc(CAMERA_WIDTH*CAMERA_HEIGHT);
    if (imgBuffL2 == NULL)
    {
        printf("Failed to allocate Memory for Image\n");
        pmsis_exit(-2);
    }

    imgBuffL2_COLOR = (uint8_t *) pmsis_l2_malloc(CAMERA_WIDTH*CAMERA_HEIGHT*3);
    if (imgBuffL2_COLOR == NULL)
    {
        printf("Failed to allocate Memory for Image\n");
        pmsis_exit(-2);
    }

    //alloc for resized picture
    imgBuffL2_TEMP = (uint8_t *) pmsis_l2_malloc(AT_INPUT_SIZE);
    if (imgBuffL2_TEMP == NULL)
    {
        printf("Failed to allocate Memory for Image\n");
        pmsis_exit(-2);
    }

    //alloc for inference picture
    printf("allocate buffer for L3\n");
    if (pi_ram_alloc(&ram, &l3_addr, (uint32_t) AT_INPUT_SIZE))
    {
        printf("Ram malloc failed !\n");
        pmsis_exit(-3);
    }

    printf("Open Himax camera\n");
    errors = open_camera_himax(&cam);
    if (errors)
    {
        printf("Failed to open camera : %ld\n", errors);
        pmsis_exit(-4);
    }

    //use loop
    //while(1)
    //{
        printf("Capture picture\n");
        pi_camera_control(&cam, PI_CAMERA_CMD_START, 0);
        pi_camera_capture(&cam, imgBuffL2, CAMERA_WIDTH*CAMERA_HEIGHT);
        pi_camera_control(&cam, PI_CAMERA_CMD_STOP, 0);

        printf("convert to color image\n");
        demosaicking(imgBuffL2, imgBuffL2_COLOR, CAMERA_WIDTH, CAMERA_HEIGHT, 0);
        
        // Image Resize to [ AT_INPUT_HEIGHT x AT_INPUT_WIDTH x [R G B] ]
        int ps=0;
        int step=10;
        for(int i=0;i<AT_INPUT_HEIGHT*step*AT_INPUT_COLORS;i+=step*AT_INPUT_COLORS){
            for(int j=0;j<AT_INPUT_WIDTH*step*AT_INPUT_COLORS;j+=step*AT_INPUT_COLORS){
                imgBuffL2_TEMP[ps] = imgBuffL2_COLOR[i*CAMERA_HEIGHT+j];
                imgBuffL2_TEMP[ps+1] = imgBuffL2_COLOR[i*CAMERA_HEIGHT+j+1];
                imgBuffL2_TEMP[ps+2] = imgBuffL2_COLOR[i*CAMERA_HEIGHT+j+2];
                ps+=3;
            }
        }
        printf("%d\n",ps);
        
        WriteImageToFile("../../../img_raw.ppm", CAMERA_HEIGHT, CAMERA_WIDTH, sizeof(uint8_t), imgBuffL2, GRAY_SCALE_IO);
        WriteImageToFile("../../../img_color.ppm", CAMERA_HEIGHT, CAMERA_WIDTH, sizeof(uint32_t), imgBuffL2_COLOR, RGB888_IO);
        WriteImageToFile("../../../img_resize.ppm", AT_INPUT_HEIGHT, AT_INPUT_WIDTH, sizeof(uint32_t), imgBuffL2_TEMP, RGB888_IO);
        
        printf("Write image to L3\n");
        pi_ram_write(&ram, l3_addr, imgBuffL2_TEMP, (uint32_t) AT_INPUT_WIDTH*AT_INPUT_HEIGHT*3);
        WriteImageToFile("../../../img_l3.ppm", AT_INPUT_HEIGHT, AT_INPUT_WIDTH, sizeof(uint32_t), (uint8_t *)l3_addr, RGB888_IO);

    #ifdef PRINT_IMAGE
        for (int i=0; i<AT_INPUT_HEIGHT; i++)
        {
            for (int j=0; j<AT_INPUT_WIDTH; j++)
            {
                printf("%03d, ", imgBuffL2_TEMP[AT_INPUT_HEIGHT*i + j]);
            }
            printf("\n");
        }
    #endif  /* PRINT_IMAGE */

        ResOut = (short int *) AT_L2_ALLOC(0, NUM_CLASSES * sizeof(short int));
        if (ResOut == NULL)
        {
            printf("Failed to allocate Memory for Result (%d bytes)\n", NUM_CLASSES*sizeof(short int));
            pmsis_exit(-5);
        }

        /* Configure And open cluster. */
        struct pi_device cluster_dev;
        struct pi_cluster_conf cl_conf;
        cl_conf.id = 0;
        pi_open_from_conf(&cluster_dev, (void *) &cl_conf);
        if (pi_cluster_open(&cluster_dev))
        {
            printf("Cluster open failed !\n");
            pmsis_exit(-6);
        }

        printf("Constructor\n");
        // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
        if (modelCNN_Construct())
        {
            printf("Graph constructor exited with an error\n");
            pmsis_exit(-7);
        }

        printf("Call cluster\n");
        struct pi_cluster_task task = {0};
        task.entry = cluster;
        task.arg = NULL;
        task.stack_size = (unsigned int) STACK_SIZE;
        task.slave_stack_size = (unsigned int) SLAVE_STACK_SIZE;

        pi_cluster_send_task_to_cl(&cluster_dev, &task);

        modelCNN_Destruct();

    #ifdef PERF  
        {
        unsigned int TotalCycles = 0, TotalOper = 0;
        printf("\n");
        for (int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
            printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", AT_GraphNodeNames[i], AT_GraphPerf[i], AT_GraphOperInfosNames[i], ((float) AT_GraphOperInfosNames[i])/ AT_GraphPerf[i]);
            TotalCycles += AT_GraphPerf[i]; TotalOper += AT_GraphOperInfosNames[i];
        }
        printf("\n");
        printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
        printf("\n");
        }
    #endif   
        pi_cluster_close(&cluster_dev);

    //}

    // Close dev
    pmsis_l2_malloc_free(imgBuffL2, CAMERA_WIDTH*CAMERA_HEIGHT);
    pmsis_l2_malloc_free(imgBuffL2_COLOR, CAMERA_WIDTH*CAMERA_HEIGHT*3);
    pmsis_l2_malloc_free(imgBuffL2_TEMP, AT_INPUT_WIDTH*AT_INPUT_HEIGHT);
    AT_L2_FREE(0, ResOut, NUM_CLASSES * sizeof(short int));
    pi_ram_free(&ram, l3_addr, (uint32_t) AT_INPUT_SIZE);

    pi_ram_close(&ram);
    pi_camera_close(&cam);

    printf("Ended\n");

    pmsis_exit(0);
    return 0;
}

int main()
{
    printf("\n\n\t *** OnBoard Inference Example ***\n\n");
    return pmsis_kickoff((void *) inference);
}