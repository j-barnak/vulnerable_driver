#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/pipe_fs_i.h>

#define MAX_SIZE 0x1000

typedef struct my_pipe_buffer {
    size_t size;
    unsigned char *buffer;
    size_t read_index;
    size_t write_index;
} my_pipe_buffer_t;

/* Define pipe structure */
typedef struct pipe {
    spinlock_t lock;
    int state;
    my_pipe_buffer_t *buffer;
} pipe_t;

/* States */
#define PIPE_UNINITIALIZED 0
#define PIPE_INITIALIZED   1

static pipe_t global_pipe;

#define IOCTL_PIPE_INIT _IOWR('p', 1, uint64_t)
#define IOCTL_PIPE_WRITE _IOWR('p', 2, struct pipe_buffer_param)
#define IOCTL_PIPE_READ _IOWR('p', 3, struct pipe_buffer_param)

struct pipe_buffer_param {
    uint64_t size;
    void * __user buffer;
};

static long pipe_ioctl_init(unsigned long arg) {
    uint64_t size = arg + sizeof(my_pipe_buffer_t);
    my_pipe_buffer_t *buf = NULL;
    int err = 0;

    if (size > MAX_SIZE) {
        return -EINVAL;
    }

    spin_lock(&global_pipe.lock);

    if (global_pipe.state != PIPE_UNINITIALIZED) {
        err = -EINVAL;
        goto out_unlock;
    }

    buf = kzalloc(size, GFP_KERNEL);
    if (!buf) {
        err = -ENOMEM;
        goto out_unlock;
    }

    global_pipe.buffer = buf;
    global_pipe.buffer->size = size - sizeof(my_pipe_buffer_t);
    global_pipe.buffer->write_index = 0;
    global_pipe.buffer->read_index = 0;
    global_pipe.state = PIPE_INITIALIZED;

    pr_info("Initialized pipe with buffer size %lx\n", global_pipe.buffer->size);

out_unlock:
    spin_unlock(&global_pipe.lock);
    return err;
}

static long pipe_write(unsigned long arg) {
    struct pipe_buffer_param param;
    size_t size;
    int err = 0;

    if (copy_from_user(&param, (void __user *)arg, sizeof(param))) {
        return -EFAULT;
    }

    size = param.size;

    spin_lock(&global_pipe.lock);

    if (global_pipe.state != PIPE_INITIALIZED) {
        err = -EINVAL;
        goto out_unlock;
    }

    if ((global_pipe.buffer->size - (global_pipe.buffer->write_index - global_pipe.buffer->read_index)) < size) {
        err = -ENOMEM;
        goto out_unlock;
    }

    if (copy_from_user(global_pipe.buffer->buffer + global_pipe.buffer->write_index, param.buffer, size)) {
        err = -EFAULT;
        goto out_unlock;
    }

    global_pipe.buffer->write_index = (global_pipe.buffer->write_index + size) % global_pipe.buffer->size;

out_unlock:
    spin_unlock(&global_pipe.lock);
    return err ? err : size;
}

static long pipe_read(unsigned long arg) {
    struct pipe_buffer_param param;
    size_t size;
    int err = 0;

    if (copy_from_user(&param, (void __user *)arg, sizeof(param))) {
        return -EFAULT;
    }

    size = param.size;

    spin_lock(&global_pipe.lock);

    if (global_pipe.state != PIPE_INITIALIZED) {
        err = -EINVAL;
        goto out_unlock;
    }

    if ((global_pipe.buffer->write_index - global_pipe.buffer->read_index) < size) {
        err = -EWOULDBLOCK;
        goto out_unlock;
    }

    if (copy_to_user(param.buffer, global_pipe.buffer->buffer + global_pipe.buffer->read_index, size)) {
        err = -EFAULT;
        goto out_unlock;
    }

    global_pipe.buffer->read_index = (global_pipe.buffer->read_index + size) % global_pipe.buffer->size;

out_unlock:
    spin_unlock(&global_pipe.lock);
    return err ? err : size;
}

static long pipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case IOCTL_PIPE_INIT:
            return pipe_ioctl_init(arg);
        case IOCTL_PIPE_WRITE:
            return pipe_write(arg);
        case IOCTL_PIPE_READ:
            return pipe_read(arg);
        default:
            return -EINVAL;
    }
}

static int pipe_open(struct inode *inode, struct file *file) {
    spin_lock_init(&global_pipe.lock);
    global_pipe.state = PIPE_UNINITIALIZED;
    global_pipe.buffer = NULL;
    return 0;
}

static int pipe_close(struct inode *inode, struct file *file) {
    spin_lock(&global_pipe.lock);
    if (global_pipe.buffer) {
        kfree(global_pipe.buffer);
    }
    global_pipe.state = PIPE_UNINITIALIZED;
    spin_unlock(&global_pipe.lock);
    return 0;
}

static const struct file_operations pipe_fops = {
    .owner = THIS_MODULE,
    .open = pipe_open,
    .release = pipe_close,
    .unlocked_ioctl = pipe_ioctl,
    .llseek = no_llseek,
};

static struct miscdevice pipe_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "pipe_dev",
    .fops = &pipe_fops,
};

static int __init pipe_init(void) {
    int ret;
    ret = misc_register(&pipe_device);
    if (ret) {
        pr_err("Failed to register device\n");
        return ret;
    }
    pr_info("Pipe device initialized\n");
    return 0;
}

static void __exit pipe_exit(void) {
    misc_deregister(&pipe_device);
    pr_info("Pipe device removed\n");
}

module_init(pipe_init);
module_exit(pipe_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("Pipe based interger underflow to gain arb. write");
