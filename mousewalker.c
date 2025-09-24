#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <threads.h>
#include <unistd.h>

#include <linux/uinput.h>
#include <sys/ioctl.h>

static void setupAbs(int fd, int type, int min, int max, int res)
{
  struct uinput_abs_setup abs = {
    .code = type,
    .absinfo = {
      .minimum = min,
      .maximum = max,
      .resolution = res
      }
  };

  if (-1 == ioctl(fd, UI_ABS_SETUP, &abs)){
    perror("ioctl UI_ABS_SETUP");
    exit(EXIT_FAILURE);
  }
}

static void init(int fd, int width, int height, int dpi)
{
    if (-1 == ioctl(fd, UI_SET_EVBIT, EV_SYN)) { perror("ioctl UI_SET_EVBIT EV_SYN"); exit(EXIT_FAILURE); }

    if (-1 == ioctl(fd, UI_SET_EVBIT, EV_KEY)) { perror("ioctl UI_SET_EVBIT EV_KEY"); exit(EXIT_FAILURE); }
    if (-1 == ioctl(fd, UI_SET_KEYBIT, BTN_LEFT)) { perror("ioctl UI_SET_KEYBIT BTN_LEFT"); exit(EXIT_FAILURE); }
    if (-1 == ioctl(fd, UI_SET_EVBIT, EV_ABS)) { perror("ioctl UI_SET_EVBIT EV_ABS"); exit(EXIT_FAILURE); }

    struct uinput_setup device = {
        .id = {
            .bustype = BUS_USB
        },
        .name = "Emulated Absolute Positioning Device"
    };

    if (-1 == ioctl(fd, UI_DEV_SETUP, &device)) { perror("ioctl UI_DEV_SETUP"); exit(EXIT_FAILURE); }

    setupAbs(fd, ABS_X, 0, width, dpi);
    setupAbs(fd, ABS_Y, 0, height, dpi);

    if (-1 == ioctl(fd, UI_DEV_CREATE)) { perror("ioctl UI_DEV_CREATE"); exit(EXIT_FAILURE); }

    sleep(1);
}

static void emit(int fd, int type, int code, int value)
{
    struct input_event ie = {
        .type = type,
        .code = code,
        .value = value
    };

    write(fd, &ie, sizeof ie);
}


void func(float_t alp, int *x, int *y){
  int r = 150;
  *x = ( (r * sqrt(2.) * cos(alp)) / ( 1 + sin(alp) * sin(alp)) );
  *y = ( (r * sqrt(2.) * cos(alp) * sin(alp)) / (1 + sin(alp) * sin(alp)) );
}

int main(int argc, char **argv)
{
    struct input_event keyboard_event;

    int w = argc > 1 ? atoi(argv[1]) : 1920;
    int h = argc > 2 ? atoi(argv[2]) : 1080;
    int d = argc > 3 ? atoi(argv[3]) : 96;
  
    if (w < 1 || h < 1 || d < 1) { perror("Bad values"); exit(EXIT_FAILURE); }

    int mfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (mfd == -1) { perror("open"); exit(EXIT_FAILURE); }

    int kfd = open("/dev/input/by-path/platform-i8042-serio-0-event-kbd", O_RDONLY | O_NONBLOCK);
    if (kfd == -1) { perror("open"); exit(EXIT_FAILURE); }

    printf("Initializing device screen map as %dx%d @ %ddpi\n", w, h, d);

    init(mfd, w, h, d);

    float_t alp = 0.;
    int cx = w / 2;
    int cy = h / 2;
    int x, y;
  

    int c = 0;
    while (1 && c < 1000)  {
      
      func(alp, &x, &y);
      x += cx; y += cy;

      ssize_t n = read(kfd, &keyboard_event, sizeof keyboard_event);  
      if (keyboard_event.type == EV_KEY && keyboard_event.value == 0 && keyboard_event.code == 1){
        break;
      }

      emit(mfd, EV_ABS, ABS_X, 1 + x);
      emit(mfd, EV_ABS, ABS_Y, 1 + y);
      emit(mfd, EV_SYN, SYN_REPORT, 0);

      alp += 0.05;
      if (alp >= 6.28){ alp = 0; }
      thrd_sleep(&(struct timespec){.tv_nsec=10*1000*1000}, NULL);
      // c++;
    }

    printf("Cleaning up...\n");

    sleep(1);

    if (-1 == ioctl(mfd, UI_DEV_DESTROY)) { perror("ioctl UI_DEV_DESTROY"); exit(EXIT_FAILURE); }

    close(mfd);
    close(kfd);
}
