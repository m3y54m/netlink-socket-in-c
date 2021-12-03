#include <sys/socket.h>      // for socket(), bind(), connect(), send(), and recv()
#include <linux/netlink.h>   // for struct sockaddr_nl, nlmsghdr
#include <linux/connector.h> // for struct cn_msg
#include <linux/cn_proc.h>   // for enum proc_cn_mcast_op, CN_NETLINK_USERSOCK
#include <signal.h>          // for signal()
#include <errno.h>           // for error codes
#include <stdbool.h>         // for bool
#include <unistd.h>          // for close()
#include <string.h>          // for memset()
#include <stdlib.h>          // for exit()
#include <stdio.h>           // for printf(), perror()

/*
 * connect to netlink
 * returns netlink socket, or -1 on error
 */
static int nl_connect()
{
    int rc;
    int nl_sock;
    struct sockaddr_nl sa_nl;

    nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (nl_sock == -1)
    {
        perror("socket");
        return -1;
    }

    sa_nl.nl_family = AF_NETLINK;
    sa_nl.nl_groups = CN_IDX_PROC;
    sa_nl.nl_pid = getpid();

    rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
    if (rc == -1)
    {
        perror("bind");
        close(nl_sock);
        return -1;
    }

    return nl_sock;
}

/*
 * subscribe on proc events (process notifications)
 */
static int set_proc_ev_listen(int nl_sock, bool enable)
{
    int rc;
    struct __attribute__((aligned(NLMSG_ALIGNTO)))
    {
        struct nlmsghdr nl_hdr;
        struct __attribute__((__packed__))
        {
            struct cn_msg cn_msg;
            enum proc_cn_mcast_op cn_mcast;
        };
    } nlcn_msg;

    memset(&nlcn_msg, 0, sizeof(nlcn_msg));
    nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
    nlcn_msg.nl_hdr.nlmsg_pid = getpid();
    nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

    nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
    nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
    nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

    nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

    rc = send(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
    if (rc == -1)
    {
        perror("netlink send");
        return -1;
    }

    return 0;
}

/*
 * handle a single process event
 */
static volatile bool need_exit = false;
static int handle_proc_ev(int nl_sock)
{
    int rc;
    struct __attribute__((aligned(NLMSG_ALIGNTO)))
    {
        struct nlmsghdr nl_hdr;
        struct __attribute__((__packed__))
        {
            struct cn_msg cn_msg;
            struct proc_event proc_ev;
        };
    } nlcn_msg;

    while (!need_exit)
    {
        rc = recv(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
        if (rc == 0)
        {
            /* shutdown? */
            return 0;
        }
        else if (rc == -1)
        {
            if (errno == EINTR)
                continue;
            perror("netlink recv");
            return -1;
        }

        switch (nlcn_msg.proc_ev.what)
        {
        case PROC_EVENT_NONE:
            printf("set mcast listen ok\n");
            break;

       /*
        * From the user's point of view, the process
        * ID is the thread group ID and thread ID is the internal
        * kernel "pid". So, fields are assigned as follow:
        *
        *  In user space     -  In  kernel space
        *
        * parent process ID  =  parent->tgid
        * parent thread  ID  =  parent->pid
        * child  process ID  =  child->tgid
        * child  thread  ID  =  child->pid
        */
        case PROC_EVENT_FORK:
            printf("fork: parent tid=%d pid=%d -> child tid=%d pid=%d\n",
                   nlcn_msg.proc_ev.event_data.fork.parent_pid,
                   nlcn_msg.proc_ev.event_data.fork.parent_tgid,
                   nlcn_msg.proc_ev.event_data.fork.child_pid,
                   nlcn_msg.proc_ev.event_data.fork.child_tgid);
            break;

        case PROC_EVENT_EXEC:
            printf("exec: tid=%d pid=%d\n",
                   nlcn_msg.proc_ev.event_data.exec.process_pid,
                   nlcn_msg.proc_ev.event_data.exec.process_tgid);
            break;

        case PROC_EVENT_UID:
            printf("uid change: tid=%d pid=%d from %d to %d\n",
                   nlcn_msg.proc_ev.event_data.id.process_pid,
                   nlcn_msg.proc_ev.event_data.id.process_tgid,
                   nlcn_msg.proc_ev.event_data.id.r.ruid,
                   nlcn_msg.proc_ev.event_data.id.e.euid);
            break;

        case PROC_EVENT_GID:
            printf("gid change: tid=%d pid=%d from %d to %d\n",
                   nlcn_msg.proc_ev.event_data.id.process_pid,
                   nlcn_msg.proc_ev.event_data.id.process_tgid,
                   nlcn_msg.proc_ev.event_data.id.r.rgid,
                   nlcn_msg.proc_ev.event_data.id.e.egid);
            break;

        case PROC_EVENT_EXIT:
            printf("exit: tid=%d pid=%d exit_code=%d\n",
                   nlcn_msg.proc_ev.event_data.exit.process_pid,
                   nlcn_msg.proc_ev.event_data.exit.process_tgid,
                   nlcn_msg.proc_ev.event_data.exit.exit_code);
            break;

        default:
            printf("unhandled proc event\n");
            break;
        }
    }

    return 0;
}

static void on_sigint(int unused)
{
    need_exit = true;
}

int main(int argc, const char *argv[])
{
    /* netlink socket */
    int nl_sock;
    /* return code */
    int rc = EXIT_SUCCESS;

    signal(SIGINT, &on_sigint);
    /*
     * deprecated:
     *
    siginterrupt(SIGINT, true);
     *  
     * alternative:
     */
    struct sigaction sa = {
        .sa_flags = SA_RESTART};
    /* Restart functions if interrupted by handler */
    sigaction(SIGINT, &sa, NULL);

    nl_sock = nl_connect();
    if (nl_sock == -1)
        exit(EXIT_FAILURE);

    rc = set_proc_ev_listen(nl_sock, true);
    if (rc == -1)
    {
        rc = EXIT_FAILURE;
    }

    rc = handle_proc_ev(nl_sock);
    if (rc == -1)
    {
        rc = EXIT_FAILURE;
    }

    if (rc == EXIT_SUCCESS)
    {
        set_proc_ev_listen(nl_sock, false);
    }
    else
    {
        close(nl_sock);
        exit(rc);
    }
}