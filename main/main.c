#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Define a task structure
typedef struct {
    TaskHandle_t taskHandle;
    int deadline;
    const char *name;
} EDFTask;

// Array to hold tasks
#define MAX_TASKS 5
EDFTask tasks[MAX_TASKS];
int taskCount = 0;

// Function to create a task
void createTask(void (*taskFunction)(void *), const char *name, int priority, int deadline) {
    if (taskCount < MAX_TASKS) {
        tasks[taskCount].deadline = deadline;
        tasks[taskCount].name = name;
        xTaskCreate(taskFunction, name, 2048, NULL, priority, &tasks[taskCount].taskHandle);
        taskCount++;
    }
}

// Example task function
void exampleTask(void *pvParameters) {
    while (1) {
        printf("Running %s\n", pcTaskGetName(NULL));
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

// EDF Scheduler function
void edfScheduler(void *pvParameters) {
    while (1) {
        // Sort tasks by deadline
        for (int i = 0; i < taskCount - 1; i++) {
            for (int j = i + 1; j < taskCount; j++) {
                if (tasks[i].deadline > tasks[j].deadline) {
                    EDFTask temp = tasks[i];
                    tasks[i] = tasks[j];
                    tasks[j] = temp;
                }
            }
        }

        // Print task order based on deadlines
        printf("Task order based on deadlines:\n");
        for (int i = 0; i < taskCount; i++) {
            printf("Task: %s, Deadline: %d\n", tasks[i].name, tasks[i].deadline);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

// Simulation task function
void simulationTask(void *pvParameters) {
    // Simulate creating tasks with different deadlines
    for (int i = 0; i < MAX_TASKS; i++) {
        char taskName[16];
        snprintf(taskName, sizeof(taskName), "Task %d", i);
        int deadline = (i + 1) * 1000; // Example deadlines
        createTask(exampleTask, taskName, 1, deadline);
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay between task creations
    }
}

void app_main(void) {
    // Start the EDF scheduler
    xTaskCreate(edfScheduler, "EDF Scheduler", 2048, NULL, 2, NULL);

    // Start the simulation task
    xTaskCreate(simulationTask, "Simulation Task", 2048, NULL, 1, NULL);
}
