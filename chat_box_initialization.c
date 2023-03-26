#include <stdio.h>

#include <stdlib.h>

#include <sys/sem.h>

#include <sys/shm.h>

#include <string.h>


#define MAX_NAME_LEN 20
#define MAX_MSG_LEN 100
#define MAX_MESSAGES 100
#define MAX_USERS 10

char name[MAX_NAME_LEN];

typedef struct {
  time_t timestamp;
  char name[MAX_NAME_LEN];
  char message[MAX_MSG_LEN];
}
chat_message;

union semun {
  int val;
  struct semid_ds * buf;
  unsigned short * array;
};

typedef struct {
  int num_users;
  char users[MAX_USERS][MAX_NAME_LEN];
  int num_messages;
  chat_message messages[MAX_MESSAGES];
}
chat_box;

int main(int argc, char * argv[]) {
  if (argc != 2) {
    printf("Usage: chat_box_initialization <Delete/DeleteThenCreate>)>\n");
    return 1;
  }

  if (strcmp(argv[1], "Delete") != 0 && strcmp(argv[1], "DeleteThenCreate") != 0) {
    printf("Invalid option. Please use 'Delete' or 'DeleteThenCreate'.\n");
    return 1;
  }

  // Create key to access shared memory area and semaphore
  key_t key = ftok("chat_box_initialization", 1);
  printf("key = %d\n", key);

  printf("Delete existing chat box...\n");

  // Remove existing semaphore if exists
  int semid = semget(key, 1, IPC_CREAT | 0666);
  if (semid != -1) {
    printf("Deleting sem, semid = %d\n", semid);
    if (semctl(semid, 0, IPC_RMID, 0) == -1) {
      perror("semctl");
      exit(1);
    }
  }

  // Remove existing shared memory if it exists
  int shmid = shmget(key, sizeof(chat_box), IPC_CREAT | 0666);
  if (shmid != -1) {
    printf("Deleting shm, shmid = %d\n", shmid);
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
      perror("shmctl");
      exit(1);
    }
  }

  // Check if need to create new chat box
  if (strcmp(argv[1], "DeleteThenCreate") != 0)
    return 0;

  printf("Creating new chat box...\n");

  // Create and initialize a new semaphore with an initial value of 1.
  semid = semget(key, 1, IPC_CREAT | 0666);
  printf("Create semid = %d\n", semid);
  if (semid == -1) {
    perror("semget");
    exit(1);
  }

  // Set the initial value of the semaphore to 1
  union semun sem_union;
  sem_union.val = 1;
  if (semctl(semid, 0, SETVAL, sem_union) == -1) {
    perror("semctl failed");
    exit(1);
  }
  printf("Initilized semaphore union value = %d\n", sem_union.val);

  // Create a new shared memory area 
  shmid = shmget(key, sizeof(chat_box), IPC_CREAT | 0666);
  printf("Create shmid = %d\n", shmid);
  if (shmid == -1) {
    perror("shmget");
    exit(1);
  }

  return 0;
}