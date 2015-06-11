#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"
#include "device.h"
#include "pal_base.h"
#include "../../pal_base_private.h"

#define STATUS_NONE      0
#define STATUS_SCHEDULED 1
#define STATUS_RUNNING   2
#define STATUS_DONE      3

struct pthreads_member {
    volatile uint32_t status;
};

static p_dev_t dev_init(struct dev *dev, int flags)
{
    int err;

    struct pthreads_dev_data *data;

    /* Be idempotent if already initialized. It might be a better idea to
     * return EBUSY instead */
    if (dev->dev_data)
        return dev;

    dev->dev_data = (void *) data;

    return dev;
}

static void dev_fini(struct dev *dev)
{
    struct pthreads_dev_data *data =
        (struct pthreads_dev_data *) dev->dev_data;

    if (!data)
        return;

    free(dev->dev_data);
    dev->dev_data = NULL;
}

static int dev_query(struct dev *dev, int property)
{
    if (!dev)
        return -EINVAL;

    switch (property) {
    case P_PROP_TYPE:
        return P_DEV_PTHREADS;
    case P_PROP_NODES:
        return sysconf(_SC_NPROCESSORS_ONLN);
    case P_PROP_TOPOLOGY:
        return 1;
    case P_PROP_SIMD:
        return 1;
    case P_PROP_MEMARCH:
    case P_PROP_WHOAMI:
        return -ENOSYS;
    }
    return -EINVAL;
}

static struct team *dev_open(struct dev *dev, struct team *team, int start,
        int count)
{
    struct pthreads_dev_data *data =
        (struct pthreads_dev_data *) dev->dev_data;

    struct pthreads_member *members;

    team->dev = dev;

    /* Initialize members status */
    members = calloc(count, sizeof(*members));
    team->data = members;

    return team;
}

static int dev_run(struct dev *dev, struct team *team, struct prog *prog,
        int start, int size, int argn, char *args[], int flags)
{
    int err;
    int i;
    struct pthreads_dev_data *data = dev->dev_data;
    struct pthreads_member *members = team->data;

    if (!data)
        return -EBADF;

    /* Set to scheduled */
    for (int i = start; i < start + size; i++)
        members[i].status = STATUS_SCHEDULED;

    /* Full memory barrier before we start threads */
    __sync_synchronize();


    /* Load */
    for (i = start; i < start + size; i++) {
        err = e_load(prog->path, &data->dev, i / 4, i % 4, E_FALSE);
        if (err)
            return -EIO;
    }

    return 0;
}


static int dev_wait(struct dev *dev, struct team *team)
{
    unsigned i;
    bool need_wait = true;
    struct epiphany_ctrl_mem ctrl;
    struct epiphany_dev_data *data =
        (struct epiphany_dev_data *) dev->dev_data;

    while (true) {
        need_wait = false;
        e_read(&data->ctrl, 0, 0, 0, &ctrl, sizeof(ctrl));
        for (i = 0; i < 16; i++) {
            switch (ctrl.status[i]) {
            case STATUS_SCHEDULED:
                /* TODO: Time out if same proc is in scheduled state too long.
                 * If program does not start immediately something has gone
                 * wrong.
                 */
            case STATUS_RUNNING:
                need_wait = true;
                break;
            case STATUS_NONE:
            case STATUS_DONE:
            default:
                continue;
            }
        }
        if (!need_wait)
            break;

        /* Don't burn CPU. Need HW/Kernel support for blocking wait */
        usleep(1000);
    }

    return 0;

}

struct dev_ops __pal_dev_pthreads_ops = {
    .init = dev_init,
    .fini = dev_fini,
    .query = dev_query,
    .open = dev_open,
    .run = dev_run,
    .wait = dev_wait
};

