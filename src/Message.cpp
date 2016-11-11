#include "Message.h"

int NamedPipe::Init()
{
    srand(time(NULL));
    if(status == NP_UNINIT) {
        char table[] = "qwertyuioplkjhgfdsazxcvbnm0987654321";
        char file[11] = {};
        for(int i = 0 ; i < 10 ; ++i) {
            file[i] = table[rand() % sizeof(table)];
        }
        sprintf(path, "/home/num/%s",file);

        int rc = mkfifo(path, 0644);
        if(rc < 0) {
            dprintf(ERROR, "mkfifo %s\n",strerror(errno));
        }

        status = NP_OPENED;
    }
    else {
        dprintf(WARN,"NamedPipe is dirty %d\n",status.load());
    }

    return 0;
}

int NamedPipe::GetReadEnd()
{
    int rc = open(path, O_RDONLY);
    if(rc < 0)
        dprintf(ERROR,"open %s\n",strerror(errno));
    read_fd = rc;
    status = NP_OK;
    return rc;
}

int NamedPipe::GetWriteEnd()
{
    /* blocks until any process open read end */
    int rc = open(path, O_WRONLY);
    if(rc < 0)
        dprintf(ERROR,"open %s\n",strerror(errno));
    write_fd = rc;
    return rc;
}

int MessageCenter::OpenReadEnd()
{
    for(int i = 0 ; i < 30 ; ++i) {
        if(data->pipe[i][self_index].status == NP_WAIT) {
            data->pipe[i][self_index].GetReadEnd();
        }
    }
    /* We opened named pipe and write end process is happy now */
    return 0;
}

int MessageCenter::GetReadEnd(int source)
{
    auto& fifo = data->pipe[source][self_index];
    if(fifo.status != NP_OK) {
        /* not exist */
        printf("*** Error: the pipe #%d->#%d does not exist yet. *** \n",source+1,self_index+1);
        return -1;
    } 

    /* when read side get fd, we tag the fifo as USING */
    fifo.status = NP_USING;

    return fifo.read_fd;
}

int MessageCenter::OpenWriteEnd(int target)
{
    /* setup everything */
    auto& fifo = data->pipe[self_index][target];

    /* test if exist */
    if(fifo.status != NP_UNINIT) {
        printf("*** Error: the pipe #%d->#%d already exists. *** \n",self_index+1,target+1);
        //dprintf(WARN,"NamedPipe is dirty %d\n",fifo.status.load());
        return -1;
    }

    fifo.Init();

    /* wait parent notify another process */
    fifo.status = NP_WAIT;
    return fifo.GetWriteEnd();
}

int MessageCenter::Notify(int i, int j)
{
    return data->pipe[i][j].status.load();
}

MessageCenter::MessageCenter()
{
    dprintf(INFO,"message pack size = %lu\n",sizeof(MessagePack));

    if(0 > (shm_fd = shm_open("xnum", O_RDWR | O_CREAT , S_IRWXU | S_IRWXG)))
        dprintf(ERROR,"shm_open %s\n",strerror(errno));
    
    if(0 > ftruncate(shm_fd, MMAP_SIZE))
        dprintf(ERROR,"ftruncate %s\n",strerror(errno));

    mmap_ptr = mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(MAP_FAILED == mmap_ptr)
        dprintf(ERROR,"mmap %s\n",strerror(errno));
    void* ptr = mmap_ptr;

    memset(mmap_ptr, 0, MMAP_SIZE);

    data = (MessagePack*)ptr;
}

MessageCenter::~MessageCenter()
{
    RemoveUser(self_index);
    munmap(mmap_ptr, MMAP_SIZE);
    close(shm_fd);
}

int MessageCenter::AddUser(int id, char *name, char *ip)
{
    if(data->ci_ptr>=CI_SIZE)
    {
        dprintf(WARN,"user list full\n");
        return -1;
    }

    int index = 0;
    /* find unused slot */
    for( ; index < data->ci_ptr ; ++index ) {
        if( data->client_info[index].online == 0 )
            break;
    }

    dprintf(DEBUG, "%d %s %s\n",id,name,ip);
    data->client_info[index].id = data->ci_ptr;
    data->client_info[index].online = 1;
    strncpy(data->client_info[index].name, name, 128);
    strncpy(data->client_info[index].ip, ip, 128);

    char buff[1024] = {};
    sprintf(buff, "*** User '%s' entered from %s. ***\n"
            ,name,ip);
    AddBroadCast(buff,-1);

    if(index == data->ci_ptr)
        data->ci_ptr++;
    return index;
}

void MessageCenter::ShowUsers(int id)
{
    printf("  <sockd> <nickname>      <IP/port>                    <indicate me>\n");
    for(int i = 0 ; i < data->ci_ptr ; ++i)
    {
        if(data->client_info[i].online) {
            printf("  %-8d%-16s%-29s",
                    i+1,
                    data->client_info[i].name, 
                    data->client_info[i].ip);
            if(i == self_index)
                printf("<- me");
            printf("\n");
        }
    }
}

void MessageCenter::RemoveUser(int id)
{
    data->client_info[id].online = 0;

    char buff[1024] = {};
    sprintf(buff, "*** User '%s' left. ***\n"
            ,data->client_info[id].name);
    AddBroadCast(buff,-1);
}

void MessageCenter::SetName(int id, const char *name)
{
    strncpy(data->client_info[id].name, name, 128);
    char buff[1024];
    sprintf(buff, "*** User from %s is named '%s'. ***\n",
            data->client_info[id].ip,name);
    AddBroadCast(buff,-1);
}

void MessageCenter::AddBroadCast(const char *msg,int from)
{
    while(data->bc.use != false);

    strncpy(data->bc.buff, msg, 1024);
    data->bc.from = from;
    data->bc.use = true;
}

int MessageCenter::GetBroadCast(char *msg, int size)
{
    if(data->bc.use == true) {
        if(data->bc.from != -1) {
            snprintf(msg, size, "*** %s yelled ***:  %s\n",
                    data->client_info[data->bc.from].name, data->bc.buff);
        } else {
            strncpy(msg, data->bc.buff, 1024);
        }
    }
    memset(data->bc.buff, 0, 1024);
    int tmp = data->bc.use == true ? 1 : 0;
    data->bc.use = false;
    return tmp;
}

int MessageCenter::Send(int id, const char *msg)
{
    auto& box = data->msg_box[self_index][id];
    while(box.ptr == 10); /* wait until buffer is not full */

    strncpy(box.buff[box.ptr], msg, 1024);
    box.ptr++;

    return 0;
}

int MessageCenter::Trans(int from, int to, int to_fd)
{
    auto& box = data->msg_box[from][to];
    for(int i = 0 ; i < box.ptr ; ++i) {
        char head[1024] = {};
        sprintf(head,"*** %s told you ***:  ",data->client_info[from].name);

        int rc = write(to_fd, head, strlen(head));
        if(rc < 0) {
            dprintf(WARN, "current write socket error %s\n",strerror(errno));
        }

        rc = write(to_fd, box.buff[i], strlen(box.buff[i]));
        memset(box.buff[i], 0, 1024);
        if(rc < 0) {
            dprintf(WARN, "current write socket error %s\n",strerror(errno));
        }
        
        write(to_fd, "\n", 1);
    }

    box.ptr = 0;

    return 0;
}

void MessageCenter::MentionNamedPipe(int to, const char* cmd)
{
    char buff[1024] = {};
    sprintf(buff, "*** %s (#%d) just piped '%s' to %s (#%d) *** \n",
        data->client_info[self_index].name,
        self_index+1,
        cmd,
        data->client_info[to].name,
        to+1);
    AddBroadCast(buff, -1);
}

void MessageCenter::UsingNamedPipe(int from, const char* cmd)
{
    char buff[1024] = {};
    sprintf(buff, "*** %s (#%d) just received from %s (#%d) by '%s' *** \n",
        data->client_info[self_index].name,
        self_index+1,
        data->client_info[from].name,
        from+1,
        cmd);
    AddBroadCast(buff, -1);
}

int MessageCenter::FreeNamedPipe(int target)
{
    for(int i = 0 ; i < 30 ; ++i) {
        auto& fifo = data->pipe[i][target];
        if(fifo.status == NP_USING) {
            if(0 > unlink(fifo.path))
                dprintf(ERROR, "unlink %s\n",strerror(errno));
            fifo.status = NP_UNINIT;
            fifo.read_fd = -1;
            fifo.write_fd = -1;
        }
    }

    return 0;
}


