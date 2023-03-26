#include <time.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/sem.h>

#include <sys/shm.h>

#include <pthread.h>

#include <sys/ipc.h>


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

typedef struct {
  int num_users;
  char users[MAX_USERS][MAX_NAME_LEN];
  int num_messages;
  chat_message messages[MAX_MESSAGES];
}
chat_box;

union semun {
  int val;
  struct semid_ds * buf;
  unsigned short * array;
};

void * display_new_messages(void * arg) {
  chat_box * box = (chat_box * ) arg;
  int cur_messages = box -> num_messages;
  while (1) {
    usleep(100000); // sleep for 100ms to avoid busy waiting
    if (box -> num_messages > cur_messages) {
      if (strcmp(box -> messages[cur_messages].name, name) != 0)
        printf(">>%sFrom %s: %s", ctime( & box -> messages[cur_messages].timestamp), box -> messages[cur_messages].name, box -> messages[cur_messages].message);
      cur_messages += 1;
    }
  }
  return NULL;
}

void lock_semaphore(int semid) {
  struct sembuf lock = {
    0,
    -1,
    SEM_UNDO
  };
  if (semop(semid, & lock, 1) == -1) {
    perror("semop lock failed");
    exit(1);
  }
  // printf("Semaphore locked\n");
}

void unlock_semaphore(int semid) {
  struct sembuf unlock = {
    0,
    1,
    SEM_UNDO
  };
  if (semop(semid, & unlock, 1) == -1) {
    perror("semop unlock failed");
    exit(1);
  }
  // printf("Semaphore unlocked\n");
}

void get_semaphore(int semid) {
  union semun sem_union;
  int val = semctl(semid, 0, GETVAL, sem_union);
  if (val == -1) {
    perror("semctl failed");
    exit(1);
  }
  printf("val = %d\n", val);
}

int main() {
  // Create key to access shared memory area and semaphore
  key_t key = ftok("chat_box_initialization", 1);
  printf("key = %d\n", key);

  // Get semaphore ID
  int semid = semget(key, 1, IPC_CREAT | 0666);
  if (semid == -1) {
    perror("semget");
    exit(1);
  }
  printf("Got semid = %d\n", semid);

  // Get shared memory ID
  int shmid = shmget(key, sizeof(chat_box), IPC_CREAT | 0666);
  if (shmid == -1) {
    perror("shmget");
    exit(1);
  }
  printf("Got shmid = %d\n", shmid);

  // Attach shared memory to process
  chat_box * box = (chat_box * ) shmat(shmid, NULL, 0);
  if (box == (chat_box * ) - 1) {
    perror("shmat");
    exit(1);
  }

  // Start display thread
  pthread_t display_thread;
  pthread_create( & display_thread, NULL, display_new_messages, (void * ) box);

  // Enter user's name
  printf("Enter your name to join the chat box: ");
  fgets(name, sizeof(name), stdin);
  name[strcspn(name, "\n")] = '\0'; // remove newline character

  // Check if name is unique
  int is_unique = 1;
  printf("Exists %d users\n", box -> num_users);
  for (int i = 0; i < box -> num_users; i++) {
    printf("user_%d=%s\n", i + 1, box -> users[i]);
    if (strcmp(box -> users[i], name) == 0) {
      is_unique = 0;
    }
  }

  if (!is_unique) {
    printf("Name is not unique. Please choose another name.\n");
    shmdt(box);
    exit(1);
  }

  // Add user to user list
  lock_semaphore(semid); // Lock semaphore to access user list
  if (box -> num_users == MAX_USERS) {
    printf("Chat box is full. Cannot add new user.\n");
    shmdt(box);
    exit(1);
  }
  strncpy(box -> users[box -> num_users], name, MAX_NAME_LEN);
  box -> num_users++;
  unlock_semaphore(semid); // Unlock semaphore

  printf("Welcome to the chat box, %s!\n", name);
  printf("Type /exit to leave the chat box.\n");

  // Loop for message
  while (1) {
    char message[MAX_MSG_LEN];
    fgets(message, sizeof(message), stdin);
    if (strcmp(message, "/exit\n") == 0) {
      break;
    }
    if (strlen(message) <= 1) { // message is empty
      continue;
    }

    // Create new message
    chat_message new_message;
    new_message.timestamp = time(NULL);
    strncpy(new_message.name, name, MAX_NAME_LEN);
    strncpy(new_message.message, message, MAX_MSG_LEN);

    // Add new message to chat box
    lock_semaphore(semid); // Lock semaphore to access user list    
    if (box -> num_messages == MAX_MESSAGES) {
      printf("Chat box is full. Cannot add new message.\n");
    } else {
      box -> messages[box -> num_messages] = new_message;
      box -> num_messages++;
    }
    unlock_semaphore(semid); // Unlock semaphore
  }

  // Remove user from user list
  lock_semaphore(semid); // Lock semaphore to access user list
  for (int i = 0; i < box -> num_users; i++) {
    if (strcmp(box -> users[i], name) == 0) {
      // Shift user list to remove user
      for (int j = i; j < box -> num_users - 1; j++) {
        strncpy(box -> users[j], box -> users[j + 1], MAX_NAME_LEN);
      }
      box -> num_users--;
      break;
    }
  }
  unlock_semaphore(semid); // Unlock semaphore
  return 0;
}