#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N_THREADS 4
#define MESSAGES 20
#define ARRAY_SIZE 20

int global_buffer;

int global_availmsg = 0;

pthread_cond_t msg_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t msg_out = PTHREAD_COND_INITIALIZER;
pthread_cond_t job_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t job_out = PTHREAD_COND_INITIALIZER;

pthread_mutex_t complete_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

struct Part {
  int low;
  int high;
  int *arr;
  int length;
} typedef Part;

struct Queue {
  int front, rear, size;
  unsigned capacity;
  Part *array;
} typedef Queue;

Part createPartition(int n) {
  Part part;
  part.low = 0;
  part.high = n - 1;
  part.arr = (int *)malloc(n * sizeof(int));
  part.length = 0;
  return part;
}

void fillPartition(Part part, int size) {
  for (int i = 0; i < size; i++) {
    int r = rand() % 20;
    part.arr[i] = r;
  }
  part.length = size;
}

Queue *createQueue(unsigned capacity) {
  Queue *queue = (Queue *)malloc(sizeof(Queue));
  queue->capacity = capacity;
  queue->front = queue->size = 0;
  queue->rear = capacity - 1;
  queue->array = (Part *)malloc(queue->capacity * sizeof(Part));
  return queue;
}

Queue *global_job_queue = createQueue(10000);

int isFull(struct Queue *queue) { return (queue->size == queue->capacity); }

int isEmpty(struct Queue *queue) { return (queue->size == 0); }

void enqueue(struct Queue *queue, Part item) {
  if (isFull(queue))
    return;
  queue->rear = (queue->rear + 1) % queue->capacity;
  queue->array[queue->rear] = item;
  queue->size = queue->size + 1;
}

Part dequeue(struct Queue *queue) {
  if (isEmpty(queue)) {
    Part p = {.high = 0, .low = 0};
    return p;
  }
  Part item = queue->array[queue->front];
  queue->front = (queue->front + 1) % queue->capacity;
  queue->size = queue->size - 1;
  return item;
}

void swap(int *a, int *b) {
  int t = *a;
  *a = *b;
  *b = t;
}

int partition(int arr[], int low, int high) {
  int pivot = arr[high];
  int i = (low - 1);

  for (int j = low; j <= high - 1; j++) {
    if (arr[j] < pivot) {
      i++;
      swap(&arr[i], &arr[j]);
    }
  }
  swap(&arr[i + 1], &arr[high]);
  return (i + 1);
}

void quickSort(int arr[], int low, int high) {
  if (low < high) {
    int pi = partition(arr, low, high);

    quickSort(arr, low, pi - 1);
    quickSort(arr, pi + 1, high);
  }
}

Part global_complete_message;
int global_avail_complete_msg = 0;

// Also sends a msg allerting every thread.
void add_new_job(Part part) {
  while (isFull(global_job_queue)) {
    pthread_cond_wait(&job_out, &jobs_mutex);
  }

  enqueue(global_job_queue, part);

  pthread_cond_signal(&job_in);

  pthread_mutex_unlock(&jobs_mutex);
}

Part take_job() {
  pthread_mutex_lock(&jobs_mutex);
  while (isEmpty(global_job_queue)) {
    pthread_cond_wait(&job_in, &jobs_mutex);
  }

  Part part = dequeue(global_job_queue);

  pthread_cond_signal(&job_out);
  pthread_mutex_unlock(&jobs_mutex);

  return part;
}

void send_complete_msg(Part msg) {

  pthread_mutex_lock(&complete_mutex);
  while (global_availmsg > 0) {
    pthread_cond_wait(&msg_out, &complete_mutex);
  }

  global_complete_message = msg;
  global_avail_complete_msg = 1;

  pthread_cond_signal(&msg_in);

  pthread_mutex_unlock(&complete_mutex);
}

Part recv_complete_msg() {

  pthread_mutex_lock(&complete_mutex);
  while (global_avail_complete_msg < 1) {

    pthread_cond_wait(&msg_in, &complete_mutex);
  }

  Part i = global_complete_message;
  global_availmsg = 0;

  pthread_cond_signal(&msg_out);

  pthread_mutex_unlock(&complete_mutex);

  return (i);
}

void *quick_sort_thread(Part part) {

  while (1) {
    Part part = take_job();
    if (part.length < 4) {
      quickSort(part.arr, part.low, part.high);
      send_complete_msg(part);
      pthread_exit(NULL);
    } else {

      int p = partition(part.arr, 0, 0);

      int *leftHalf = malloc(p * sizeof(int));
      int *rightHalf = malloc(part.length - p * sizeof(int));

      memcpy(leftHalf, part.arr, p * sizeof(int));
      memcpy(rightHalf, part.arr + part.length - p,
             part.length - p * sizeof(int));

      Part left_half = {.low = 0, .high = p - 1, .length = p, .arr = leftHalf};
      Part right_half = {.low = p + 1,
                         .high = part.length,
                         .length = part.length - p,
                         .arr = rightHalf};

      add_new_job(left_half);
      add_new_job(right_half);
    }
  };
}

int main() {

  pthread_t threads[N_THREADS];
  int part_size = (int)(ARRAY_SIZE / N_THREADS);
  Part parts[N_THREADS];

  for (int i = 0; i < N_THREADS; i++) {
    parts[i] = createPartition(part_size);
    fillPartition(parts[i], part_size);
    pthread_t current_thread = threads[i];
    pthread_create(&current_thread, NULL, quick_sort_thread, parts[i]);
  }

  int sorted_array[10];

  int arr_size = 10;
  int complete_size = 0;

  while (1) {

    Part msg = recv_complete_msg();

    int received_array_size = msg.high - msg.low;
    complete_size += arr_size;

    // TODO: is there a smarter way to do this ?
    int iterations = 0;
    for (int i = msg.low; i < msg.high; i++) {
      sorted_array[i] = msg.arr[iterations];
    }

    // Array has been sorted
    if (complete_size == arr_size) {
      printf("Array sorted !\n");
      for (int i = 0; i < 10; i++) {
        printf("%i, ", sorted_array[i]);
      }

      break;
    }
  }

  for (int i = 0; i < N_THREADS; i++) {
    pthread_t current_thread = threads[i];
    pthread_join(current_thread, NULL);
  }

  pthread_mutex_destroy(&complete_mutex);
  pthread_mutex_destroy(&jobs_mutex);

  pthread_cond_destroy(&job_out);
  pthread_cond_destroy(&job_in);
  pthread_cond_destroy(&msg_out);
  pthread_cond_destroy(&msg_in);

  for (int i = 0; i < N_THREADS; i++) {
    free(parts[i].arr);
  }

  return 0;
}