
// for memset
#include<string.h>
// for calloc
#include<stdlib.h>
// for printf and fwrite
#include<stdio.h>
// for read
#include<unistd.h>
// for open/close:
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>
// for ioctl (special file manipulation)
#include<sys/ioctl.h>
// for v4l2 structs
#include <linux/videodev2.h>
// for mmap
#include <sys/mman.h>
// for testing
#include <errno.h>
#include "gra.h"

struct {
    void *start;
    size_t length;
} *buffers = NULL;
struct v4l2_requestbuffers req;
int fd = -1;
int file_params = 0;
struct v4l2_capability cap;
struct v4l2_format fmt;
int width;
int height;
int imgSize;
int type;
int n_buffers = 0;// used later for dealloc (kinda not necessary)
// this was the old: int main(int argc, char **argv)
void cleanupCamera();
void initializeCameraStream()
{
    n_buffers = 0;// used later for dealloc (kinda not necessary)
    // used to tell the driver we're gunna need some buffers
    memset(&req, 0, sizeof(req));
    req.count = 4;// 4 buffers to be exact
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    // open the video file
    fd = -1;
    file_params = 0;
    file_params |= O_RDWR;
    file_params |= O_NONBLOCK;
    fd = open("/dev/video0", file_params, 0);
    if (fd == -1)
    {
        printf("couldnt open video0 err: %i, %s\n", errno,strerror(errno));
    }
   
    // let's get some info about the camera (this is kinda useless code, but is an example)
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        printf("couldnt get capabilities\n");
        cleanupCamera();
        return;
    }
    if (!(cap.capabilities & V4L2_CAP_READWRITE))
    {
        printf("cant straight io with video\n");
        cleanupCamera();
        return;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("does not support streaming i/o\n");
        cleanupCamera();
        return;
    }
    memset(&fmt, 0, sizeof(fmt));
    // set some defaults for the camera formatting
    fmt.fmt.pix.width             = 640; 
    fmt.fmt.pix.height            = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field             = V4L2_FIELD_INTERLACED;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        printf("couldnt get format\n");
        cleanupCamera();
        return;
    }
    // this is the results:
    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;
    imgSize = fmt.fmt.pix.sizeimage;
    // let's let everyone know what they already know!
    printf("video says: width: %i, height %i\n", width, height);

    
   /* idk wut this is lolz
    */
    /*
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == ioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; // reset to default

        if (-1 == ioctl(fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    // Cropping not supported. 
                    break;
                default:
                    // Errors ignored.
                    break;
            }
        }
    } else {                
        // Errors ignored. 
    }
*/
    // set the stream type
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(fd, VIDIOC_STREAMON, &type))
    {
        printf("set stream type err\n");
        cleanupCamera();
        return;
    }


    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req)) {
        printf("cant req bufs\n");
        cleanupCamera();
        return;
    }
    // set up the buffers, this video device driver uses memory mapping to get the video data to c programs using the driver.
    // The driver sets up buffers when you give it the "requestbuffers" object earlier. Now we need to know where they are in memory 
    buffers = calloc(req.count, sizeof(*buffers));
    n_buffers = 0;// used later for dealloc (kinda not necessary)
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        
        // figure out where the device buffers are in memory
        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf))
        {
            printf("couldn't find the bufs\n");
            cleanupCamera();
            return;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
        mmap (NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */, fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
        {
            printf("map failed\n");
            cleanupCamera();
            return;
        }
    }
    // now we tell the driver it can start putting info into the buffers
    for (int i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory            = V4L2_MEMORY_MMAP;
        buf.index             = i;

        if (-1 == ioctl(fd, VIDIOC_QBUF, &buf))
        {
            printf("couldn't queue\n");
            cleanupCamera();
            return;
        }
    }
}

void runCamera()
{

    for (;;)
    {
        /*
        fd_set fds;
        struct timeval tv;
        int r;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        // Timeout. 
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (-1 == select(fd + 1, &fds, NULL, NULL, &tv))
        {
            printf("too fast\n");
            continue;
        }
        */

         struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (-1 == ioctl(fd, VIDIOC_DQBUF, buf))
        {
            printf("couldn't read video\n");
            cleanupCamera();
            return;
        }
        //fwrite(buffers[buf.index].start, buffers[buf.index].length, 1, stdout); 
        if (buf.index >= n_buffers)
        {
            printf("buf idx too high\n");
        }
        printf("bytes used: %i\n", buf.bytesused);
        imageProcess(buffers[buf.index].start, width, height);
        

        if (-1 == ioctl(fd, VIDIOC_QBUF, &buf))
        {
            printf("couldn't continue reading video\n");
            cleanupCamera();
            return;
        }
        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == ioctl(fd, VIDIOC_STREAMON, &type))
        {
            printf("Couldn't keep typing");
            cleanupCamera();
            return;
        }
        break;
    }
}
void cleanupCamera()
{

    for (int i = 0; i < n_buffers; ++i)
        if (-1 == munmap (buffers[i].start, buffers[i].length))
        {
            printf("couldn't unmap\n");
        }
    if (buffers != NULL)
    {
        free(buffers);
        buffers = NULL;
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (fd == -1 || -1 == ioctl(fd, VIDIOC_STREAMOFF, &type))
    {
        printf("couldn't reset stream type\n");
    }
    close(fd);
}
int main(int argc, char **argv)
{
   initializeCameraStream();
   for (int i = 0; i < 100; ++i)
   {
      runCamera();
   }
   cleanupCamera();
}
