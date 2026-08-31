#include "platform.h"
#include "camera_info.h"
#include "color.h"
#include "keyboard.h"
#include "ptp_chdk.h"
#include "core.h"
#include "task.h"
#include "script.h"
#include "action_stack.h"
#include "live_view.h"
#include "meminfo.h"
#include "modules.h"
#include "callfunc.h"
#include "stdio.h"

#include "remotecap_core.h"

int get_ptp_file_buf_size(void);
char *get_ptp_file_buf(void);

// process id for scripts, increments before each attempt to run script
// does not handle wraparound
static unsigned script_run_id;

static int handle_ptp(
                int h, ptp_data *data, int opcode, int sess_id, int trans_id,
                int param1, int param2, int param3, int param4, int param5);

static void init_chdk_ptp()
{
  int r;

  // wait until ptp_handlers_info is initialised and add CHDK PTP interface
  r = 0x17;
  while ( r==0x17 )
  {
    r = add_ptp_handler(PTP_OC_CHDK,handle_ptp,0);
    msleep(250);
  }

  ExitTask();
}

void init_chdk_ptp_task()
{
    CreateTask("InitCHDKPTP", 0x19, 0x200, init_chdk_ptp);
}

typedef struct {
    char *buf; // buffer for transfer
    char *dst_buf; // final destination for data, if not used directly from transfer buffer
    int buf_size; // transfer buffer size (= total size if it fits)
    int total_size; // total data size
    int total_read; // total data read so far
    int last_read; // last data transfer / length of valid data in buf
} recv_ptp_data_state_t;


// define to limit receive buffer size, for testing issues with specific sizes
//#define PTP_RECV_BUF_MAX_SIZE (256*1024)

/*
initialize receive data state
dst_buf may be NULL if caller will use rs->buf directly (e.g. for file transfer where date is only needed during transaction)
 */
static int recv_ptp_data_init(recv_ptp_data_state_t *rs, int total_size, char *dst_buf)
{
    memset(rs,0,sizeof(recv_ptp_data_state_t));
#ifdef CAM_PTP_USE_NATIVE_BUFFER
    rs->buf_size = (get_ptp_file_buf_size() >> 1); // canon code seems to use half reported size for file buf
    rs->buf = get_ptp_file_buf();
#else
    //if using malloc, half of largest free block
    rs->buf_size = (core_get_free_memory() >> 1);
#endif
    // for testing smaller / fixed size chunks
#ifdef PTP_RECV_BUF_MAX_SIZE
    if(rs->buf_size > PTP_RECV_BUF_MAX_SIZE) {
        rs->buf_size = PTP_RECV_BUF_MAX_SIZE;
    }
#endif
    // clamp size is a multiple of 512 to simplify multi-read workaround (0x1f5 bug)
    // https://chdk.setepontos.com/index.php?topic=4338.msg140577#msg140577
    // size must also be multiple of 4 due to other issues on some cameras
    // http://chdk.setepontos.com/index.php?topic=6730.msg76760#msg76760
    rs->buf_size &= 0xFFFFFE00;
    // things will be badly broken if only a few KB free, try to fail gracefully
    if(rs->buf_size < 2048) {
        return 0;
    }
    // requested size smaller that buffer
    if(rs->buf_size > total_size) {
        rs->buf_size = total_size;
    }
    rs->total_size = total_size;
    rs->total_read = 0;
    rs->last_read = 0;
    rs->dst_buf = dst_buf;

#ifndef CAM_PTP_USE_NATIVE_BUFFER
    if(!dst_buf) {
        rs->buf = malloc(rs->buf_size);
        if(!rs->buf) {
            return 0;
        }
    } else {
        rs->buf = NULL;
    }
#endif
    return 1;
}

/*
read a chunk of data, based on size and buffer configuration from recv_ptp_data_chunk
may be called multiple times per transaction
*/
static int recv_ptp_data_chunk(recv_ptp_data_state_t *rs,ptp_data *data)
{
    int size_left = rs->total_size - rs->total_read;
    if(size_left <= 0) {
        return 0;
    }
#ifndef CAM_PTP_USE_NATIVE_BUFFER
    // using unbuffered, has to be all in one go
    if(!rs->buf) {
        // TODO maybe shouldn't add to total on error?
        rs->total_read = rs->last_read = rs->total_size;
        if(data->recv_data(data->handle,rs->dst_buf,rs->total_size,0,0) != 0) {
            return 0;
        }
        return 1;
    }
#endif

    int rsize;
    // less than one chunk remaining, read all
    if(size_left <= rs->buf_size) {
        rsize = size_left;
    } else {
        rsize = rs->buf_size;
        // if on last full chunk, check for multi-read bug sizes
        // https://chdk.setepontos.com/index.php?topic=4338.msg140577#msg140577
        if(size_left <= rs->buf_size * 2) {
            // assumes buf_size is multiple of 512, enforced in recv_ptp_data_init
            int rest = size_left % rs->buf_size;
            // if final transfer would be problem size, reduce size of next to last transfer by 1k
            // if buffer size is less than 0x800, things are badly hosed anyway
            if(rs->buf_size >= 0x800 && rest > 0x1f4 && rest < 0x3f4) {
                rsize -= 0x400;
            }
        }
    }
    rs->last_read = rsize;
    if(data->recv_data(data->handle,rs->buf,rsize,0,0) != 0) {
        return 0;
    }
    if(rs->dst_buf) {
        memcpy(rs->dst_buf + rs->total_read,rs->buf,rsize);
    }
    rs->total_read += rsize;
    return 1;
}

/*
free buffer if needed
*/
static void recv_ptp_data_finish(recv_ptp_data_state_t *rs)
{
#ifndef CAM_PTP_USE_NATIVE_BUFFER
    free(rs->buf);
#endif
    memset(rs,0,sizeof(recv_ptp_data_state_t));
}

/*
read size into buf
use to read full transaction data into buf, or discard if buf is NULL
to read and process chunks directly (i.e. for file transfer) use recv_ptp_data_chunk
*/
static int recv_ptp_data(ptp_data *data, char *buf, int size)
{
    recv_ptp_data_state_t rs;
    if(!recv_ptp_data_init(&rs,size,buf)) {
        return 0;
    }
    int status=1;
    while(rs.total_read < size && status) {
        status = recv_ptp_data_chunk(&rs,data);
    }
    recv_ptp_data_finish(&rs);
    return status;
}
static void flush_recv_ptp_data(ptp_data *data,int size) {
    recv_ptp_data(data,NULL,size);
}

typedef struct {
    char *buf;
    int size;
} send_ptp_data_buf_t;

/*
initialize a buffer of up to 1/2 largest free heap block
note this could be extended to use native buffers like recv
currently not required but potentially useful on low mem cams
uncached is also faster on some
*/
static int send_ptp_data_buf_init(send_ptp_data_buf_t *sb,int total_size)
{
    int buf_size=(core_get_free_memory()>>1);
  // make sure size is an integer number of words (avoid some possible issues with multiple calls)
    buf_size &= 0xFFFFFFFC;
    if(buf_size > total_size) {
        buf_size = total_size;
    }
    sb->buf=malloc(buf_size);
    if(!sb->buf) {
        sb->size=0;
        return 0;
    }
    sb->size=buf_size;
    return 1;
}

/*
free buffer from send_ptp_data_buf_init
*/
static void send_ptp_data_buf_free(send_ptp_data_buf_t *sb)
{
    free(sb->buf);
    sb->buf=NULL;
    sb->size=0;
}

/*
send size data from src all in one go
repeated calls per transaction are *not* ok, use data->send_data directly or send_ptp_data_buffered in that case
*/
static int send_ptp_data(ptp_data *data, const char *src, int size)
{
    if ( data->send_data(data->handle,src,size,size,0,0,0) )
    {
      return 0;
    }
    return 1;
}

/*
send data buffered, for regions which cannot be directly used by DMA (MMIO, some TCMs)
copy_fn should behave like memcpy.
A function that does word by word copy rather than LDM/STM might be more correct for MMIO, but memcpy seems to work
*/
static int send_ptp_data_buffered(ptp_data *data, void * (*copy_fn)(void *d, const void *s, long sz), const char *src, int size)
{
    send_ptp_data_buf_t sb;
    if(!send_ptp_data_buf_init(&sb,size)) {
        return 0;
    }

    int tmpsize = size;
    int send_size;
    while ( size > 0 )
    {
        if(size > sb.size) {
            send_size = sb.size;
        } else {
            send_size = size;
        }
        // src inside buf ?
        if(src >= sb.buf && src < sb.buf + send_size) {
            // send whatever is in buffer without attempting to copy
            if(src + size < sb.buf + sb.size) {
                // all remainder is in buf
                send_size = size;
            } else {
                // send up to end of buffer
                send_size = sb.size - (src - sb.buf);
            }
        } else {
            // full copy size would overlap
            if(src < sb.buf && src + send_size > sb.buf) {
                // copy up to start of buf
                send_size = sb.buf - src;
            }
            copy_fn(sb.buf,src,send_size);
        }
        if ( data->send_data(data->handle,sb.buf,send_size,tmpsize,0,0,0) )
        {
            return 0;
        }
        tmpsize = 0;
        size -= send_size;
        src += send_size;
    }
    send_ptp_data_buf_free(&sb);
    return 1;
}

// TODO this could be a generic ring buffer of words
#define PTP_SCRIPT_MSG_Q_LEN 16
typedef struct {
  unsigned r; // index of current read value
  unsigned w; // index of next write value, if w == r, empty TODO "full" currently wastes a slot
  ptp_script_msg *q[PTP_SCRIPT_MSG_Q_LEN];
} ptp_script_msg_q;

// TODO it would be better to allocate these only when needed
ptp_script_msg_q msg_q_in; // messages to pc from script
ptp_script_msg_q msg_q_out; // messages to script from pc

unsigned script_msg_q_next(unsigned i) {
  if(i == PTP_SCRIPT_MSG_Q_LEN - 1) {
    return 0;
  }
  return i+1;
}

unsigned script_msg_q_full(ptp_script_msg_q *q) {
  return (script_msg_q_next(q->w) == q->r);
}

unsigned script_msg_q_empty(ptp_script_msg_q *q) {
  return (q->w == q->r);
}

int enqueue_script_msg(ptp_script_msg_q *q,ptp_script_msg *msg) {
  unsigned w = script_msg_q_next(q->w);
  if(w == q->r) {
    return 0;
  }
  if(msg == NULL) {
    return 0;
  }
  q->q[q->w] = msg;
  q->w = w;
  return 1;
}

ptp_script_msg* dequeue_script_msg(ptp_script_msg_q *q) {
  ptp_script_msg *msg;
  if(script_msg_q_empty(q)) {
    return NULL;
  }
  msg = q->q[q->r];
  q->r = script_msg_q_next(q->r);
  return msg;
}

// remove all messages from queue
void empty_script_msg_q(ptp_script_msg_q *q) {
    ptp_script_msg *msg;
    while((msg = dequeue_script_msg(q))) {
        free(msg);
    }
}

// public interface for script
// create a message to be queued later
ptp_script_msg* ptp_script_create_msg(unsigned type, unsigned subtype, unsigned datasize, const void *data) {
  ptp_script_msg *msg;
  msg = malloc(sizeof(ptp_script_msg) + datasize);
  msg->size = datasize;
  msg->type = type;
  msg->subtype = subtype;
  // caller may fill in data themselves
  // datasize may be empty (e.g. empty string)
  if(data && datasize) {
      memcpy(msg->data,data,msg->size);
  }
  return msg;
}

// add a message to the outgoing queue
int ptp_script_write_msg(ptp_script_msg *msg) {
  msg->script_id = script_run_id;
  return enqueue_script_msg(&msg_q_out,msg);
}

// retrieve the next message in the incoming queue
ptp_script_msg* ptp_script_read_msg(void) {
  ptp_script_msg *msg;
  while(1) {
    msg = dequeue_script_msg(&msg_q_in);
    // no messages
    if(!msg) {
        return NULL;
    }
    // does message belong to our script
    if(!msg->script_id || msg->script_id == script_run_id) {
      return msg;
    } else {
    // no: discard and keep looking
      free(msg);
    }
  }
}

// convenience function write an error message
int ptp_script_write_error_msg(unsigned errtype, const char *err) {
  if(script_msg_q_full(&msg_q_out)) {
    return 0;
  }
  ptp_script_msg *msg = ptp_script_create_msg(PTP_CHDK_S_MSGTYPE_ERR,errtype,strlen(err),err);
  if(!msg) {
    return 0;
  }
  return ptp_script_write_msg(msg);
}

static char*   ptp_script = 0;
static int     ptp_script_state = 0;

// Called from kbd process to start PTP script
// Load Lua module, parse PTP script and start execution if parse ok.
void start_ptp_script()
{
    if (ptp_script)
    {
        // Keyboard init
        camera_info.state.kbd_last_clicked = 0;
        camera_info.state.kbd_last_checked_time = get_tick_count();
        kbd_key_release_all();
        module_set_script_lang(0);  // Force Lua script language
        if (libscriptapi->script_start(ptp_script,1))
        {
            camera_info.state.auto_started = 0;
            ptp_script_state = script_stack_start();
        }
        else
            ptp_script_state = -1;
        ptp_script = 0;
    }
}

// Setup for kbd task to start script, wait until started and return startup state
static long script_start_ptp( char *script )
{
    ptp_script = script;
    while (ptp_script) msleep(10);
    return ptp_script_state;
}

static int handle_ptp(
               __attribute__ ((unused))int h, ptp_data *data, __attribute__ ((unused))int opcode, int sess_id, int trans_id,
               int param1, int param2, int param3, int param4, __attribute__ ((unused))int param5)
{
  static union {
    char *str;
  } temp_data;
  static int temp_data_kind = 0; // 0: nothing, 1: ascii string
  static int temp_data_extra; // size (ascii string)
  PTPContainer ptp;

  // initialise default response
  memset(&ptp,0,sizeof(PTPContainer));
  ptp.code = PTP_RC_OK;
  ptp.sess_id = sess_id;
  ptp.trans_id = trans_id;
  ptp.num_param = 0;

  // handle command
  switch ( param1 )
  {

    case PTP_CHDK_Version:
      ptp.num_param = 2;
      ptp.param1 = PTP_CHDK_VERSION_MAJOR;
      ptp.param2 = PTP_CHDK_VERSION_MINOR;
      break;
    case PTP_CHDK_ScriptSupport:
      ptp.num_param = 1;
      ptp.param1 = 0;
      ptp.param1 |= PTP_CHDK_SCRIPT_SUPPORT_LUA;
      break;
    case PTP_CHDK_ScriptStatus:
      ptp.num_param = 1;
      ptp.param1 = 0;
      ptp.param1 |= script_is_running()?PTP_CHDK_SCRIPT_STATUS_RUN:0;
      ptp.param1 |= (!script_msg_q_empty(&msg_q_out))?PTP_CHDK_SCRIPT_STATUS_MSG:0;
      break;
    case PTP_CHDK_GetMemory:
    {
      char *src=(char *)param2;
      int size=param3;
      int result=0;
      if ( size < 1 ) // invalid size? NULL is accepted
      {
        ptp.code = PTP_RC_GeneralError;
        break;
      }

      if (param4 == PTP_CHDK_GETMEM_MODE_DIRECT) {
        int total_size = size;
        // canon data->send_data fails on NULL, send first word separately
        // DMA from addresses occupied by TCM is suspect but seems to work on many cams
        // can't directly check NULL https://chdk.setepontos.com/index.php?topic=13101.0
        if((unsigned)param2 < 4 ) {
            char x[4];
            int send_size = 4 - param2;
            if(send_size > size) {
                send_size = size;
            }
            memcpy(x,src,send_size);

            // 0 is success
            if(data->send_data(data->handle,x,send_size,total_size,0,0,0) != 0) {
                ptp.code = PTP_RC_GeneralError;
                break;
            }
            // that was all, done
            if(size == send_size) {
                break;
            }
            size -= send_size;
            // total only sent on first send
            total_size = 0;
            src+=send_size;
        }
        // no need to send through send_ptp, faster with one call
        if(data->send_data(data->handle,src,size,total_size,0,0,0) == 0) {
            result = 1;
        }
      } else if(param4 == PTP_CHDK_GETMEM_MODE_BUFFER) {
        result = send_ptp_data_buffered(data,memcpy,src,size);
      } // else error
      if(!result)
      {
        ptp.code = PTP_RC_GeneralError;
      }
      break;
    }
    case PTP_CHDK_SetMemory:
      if ( param2 == 0 || param3 < 1 ) // null pointer or invalid size?
      {
        ptp.code = PTP_RC_GeneralError;
        break;
      }

      data->get_data_size(data->handle); // XXX required call before receiving
      if ( !recv_ptp_data(data,(char *) param2,param3) )
      {
        ptp.code = PTP_RC_GeneralError;
      }
      break;

    case PTP_CHDK_CallFunction:
      {
        int s = data->get_data_size(data->handle);
        if (s <= 0 || (s&3)) // no data or not an integer number of args
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }
        unsigned *buf = malloc(s);

        if ( buf == NULL )
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }

        if ( recv_ptp_data(data,(char *) buf,s) )
        {
          ptp.num_param = 1;
          ptp.param1 = call_func_ptr((void *)buf[0],(unsigned *)buf+1,(s-4)/4);
        } else {
          ptp.code = PTP_RC_GeneralError;
        }

        free(buf);
        break;
      }

    case PTP_CHDK_TempData:
      if ( param2 & PTP_CHDK_TD_DOWNLOAD )
      {
        const char *s = NULL;
        size_t l = 0;

        if ( temp_data_kind == 0 )
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }

        if ( temp_data_kind == 1 )
        {
          s = temp_data.str;
          l = temp_data_extra;
        }

        if ( !send_ptp_data(data,s,l) )
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }

      } else if ( ! (param2 & PTP_CHDK_TD_CLEAR) ) {
        if ( temp_data_kind == 1 )
        {
          free(temp_data.str);
        }
        temp_data_kind = 0;

        temp_data_extra = data->get_data_size(data->handle);

        temp_data.str = (char *) malloc(temp_data_extra);
        if ( temp_data.str == NULL )
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }

        if ( !recv_ptp_data(data,temp_data.str,temp_data_extra) )
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }
        temp_data_kind = 1;
      }
      if ( param2 & PTP_CHDK_TD_CLEAR )
      {
        if ( temp_data_kind == 1 )
        {
          free(temp_data.str);
        }
        temp_data_kind = 0;
      }
      break;

    case PTP_CHDK_UploadFile:
      {
        FILE *f=NULL;
        char *fn=NULL;
        unsigned data_size,fn_len;
        recv_ptp_data_state_t rs;
        data_size = data->get_data_size(data->handle);
        // flush data would try to do init and fail again, so just break
        if(!recv_ptp_data_init(&rs,data_size,NULL)) {
            ptp.code = PTP_RC_GeneralError;
            break;
        }
        int recv_err = 0;
        while ( data_size > 0 ) {
            if(!recv_ptp_data_chunk(&rs,data)) {
                ptp.code = PTP_RC_GeneralError;
                recv_err = 1; // flag PTP errors to avoid trying to read more in flush
                break;
            }
            // first chunk, extact file name and write remaining data
            if(!f) {
                fn_len = *(unsigned *)rs.buf;
                fn = malloc(fn_len+1);
                if(!fn) {
                    ptp.code = PTP_RC_GeneralError;
                    break;
                }
                memcpy(fn,rs.buf+4,fn_len);
                fn[fn_len] = 0;
                f = fopen(fn,"wb");
                free(fn);
                if(!f) {
                    ptp.code = PTP_RC_GeneralError;
                    break;
                }
                fwrite(rs.buf+4+fn_len,1,rs.last_read - 4 - fn_len,f);
            } else {
                fwrite(rs.buf,1,rs.last_read,f);
            }
            data_size -= rs.last_read;
        }

        if(f) {
            fclose(f);
        }
        recv_ptp_data_finish(&rs);
        if(data_size > 0 && ptp.code != PTP_RC_OK && !recv_err) {
            flush_recv_ptp_data(data,data_size);
        }
        break;
      }

    case PTP_CHDK_DownloadFile:
      {
        FILE *f;
        int tmp,t,total_size,r;
        char *fn;

        if ( temp_data_kind != 1 )
        {
          // send dummy data, otherwise error hoses connection
          send_ptp_data(data,"\0",1);
          ptp.code = PTP_RC_GeneralError;
          break;
        }

        fn = (char *) malloc(temp_data_extra+1);
        if ( fn == NULL )
        {
          // send dummy data, otherwise error hoses connection
          send_ptp_data(data,"\0",1);
          free(temp_data.str);
          temp_data_kind = 0;
          ptp.code = PTP_RC_GeneralError;
          break;
        }
        memcpy(fn,temp_data.str,temp_data_extra);
        fn[temp_data_extra] = '\0';

        free(temp_data.str);
        temp_data_kind = 0;

        f = fopen(fn,"rb");
        if ( f == NULL )
        {
          // send dummy data, otherwise error hoses connection
          send_ptp_data(data,"\0",1);
          ptp.code = PTP_RC_GeneralError;
          free(fn);
          break;
        }
        free(fn);

        fseek(f,0,SEEK_END);
        total_size = ftell(f);
        fseek(f,0,SEEK_SET);

        send_ptp_data_buf_t sb;
        if(!send_ptp_data_buf_init(&sb,total_size))
        {
          // send dummy data, otherwise error hoses connection
          send_ptp_data(data,"\0",1);
          ptp.code = PTP_RC_GeneralError;
          fclose(f);
          break;
        }

        tmp = total_size;
        t = total_size;
        while ( (r = fread(sb.buf,1,(t<sb.size)?t:sb.size,f)) > 0 )
        {
          t -= r;
          data->send_data(data->handle,sb.buf,r,tmp,0,0,0);
          tmp = 0;
        }
        fclose(f);
        // XXX check that we actually read/send total_size bytes! (t == 0)

        ptp.num_param = 1;
        ptp.param1 = total_size;

        send_ptp_data_buf_free(&sb);

        break;
      }
      break;

    case PTP_CHDK_ExecuteScript:
      {
        int s;
        char *buf;

        ptp.num_param = 2;
        ptp.param1 = script_run_id; // in error case, ID of most recent script

        s = data->get_data_size(data->handle);

        if ( (param2&PTP_CHDK_SL_MASK) != PTP_CHDK_SL_LUA )
        {
          flush_recv_ptp_data(data,s);
          ptp.code = PTP_RC_ParameterNotSupported;
          break;
        }

        buf = (char *) malloc(s+1);
        if ( buf == NULL )
        {
          ptp.code = PTP_RC_GeneralError;
          break;
        }

        recv_ptp_data(data,buf,s);
        buf[s] = 0;

        // applies to both running and "interrupted" state, since interrupted means running restore
        if (camera_info.state.state_kbd_script_run) {
            // note script ID is still incremented in this case
            if (param2 & PTP_CHDK_SCRIPT_FL_NOKILL) {
                // no message is added in this case, since the running script might also be doing
                // stuff with messages
                ptp.param2 = PTP_CHDK_S_ERR_SCRIPTRUNNING;
                free(buf);
                break;
            }
            // kill the script
            script_wait_terminate();
        }
        // empty message queues if requested.
        if(param2 & PTP_CHDK_SCRIPT_FL_FLUSH_CAM_MSGS) {
            empty_script_msg_q(&msg_q_out);
        }
        // Script either was not running or has been killed, so safe to remove from inbound queue outside of kbd task
        if(param2 & PTP_CHDK_SCRIPT_FL_FLUSH_HOST_MSGS) {
            empty_script_msg_q(&msg_q_in);
        }

        // increment script ID if script is loaded
        script_run_id++;
        ptp.param1 = script_run_id;

        // error details will be passed in a message
        if (script_start_ptp(buf) < 0) {
          ptp.param2 = PTP_CHDK_S_ERRTYPE_COMPILE;
        } else {
          ptp.param2 = PTP_CHDK_S_ERRTYPE_NONE;
        }

        free(buf);

        break;
      }
    case PTP_CHDK_ReadScriptMsg:
    {
      char *pdata="";
      unsigned datasize=1;

      ptp_script_msg *msg = dequeue_script_msg(&msg_q_out);
      ptp.num_param = 4;
      if(msg) {
        ptp.param1 = msg->type;
        ptp.param2 = msg->subtype;
        ptp.param3 = msg->script_id;
        ptp.param4 = msg->size;
        // empty messages must have a data phase, so use default if no data
        if(msg->size) {
            datasize = msg->size;
            pdata = msg->data;
        }
	  } else {
        // return a fully formed message for easier handling
        ptp.param1 = PTP_CHDK_S_MSGTYPE_NONE;
        ptp.param2 = 0;
        ptp.param3 = 0;
        ptp.param4 = 0;
      }

      // NOTE message is lost if sending failed
      if ( !send_ptp_data(data,pdata,datasize) )
      {
        ptp.code = PTP_RC_GeneralError;
      }
      free(msg);
      break;
    }
    case PTP_CHDK_WriteScriptMsg:
    {
      int msg_size;
      ptp_script_msg *msg;
      ptp.num_param = 1;
      ptp.param1 = PTP_CHDK_S_MSGSTATUS_OK;
      if (!script_is_running()) {
        ptp.param1 = PTP_CHDK_S_MSGSTATUS_NOTRUN;
      } else if(param2 && (unsigned)param2 != script_run_id) {// check if target script for message is running
        ptp.param1 = PTP_CHDK_S_MSGSTATUS_BADID;
      } else if(script_msg_q_full(&msg_q_in)) {
        ptp.param1 = PTP_CHDK_S_MSGSTATUS_QFULL;
      }

      msg_size = data->get_data_size(data->handle);

      // if something was wrong, don't bother creating message, just flush
      if(ptp.param1 != PTP_CHDK_S_MSGSTATUS_OK) {
        flush_recv_ptp_data(data,msg_size);
        break;
      }
      msg = ptp_script_create_msg(PTP_CHDK_S_MSGTYPE_USER,PTP_CHDK_TYPE_STRING,msg_size,NULL);
      if ( !msg ) // malloc error or zero size
      {
        // if size is zero, things will get hosed no matter what
        flush_recv_ptp_data(data,msg_size);
        ptp.code = PTP_RC_GeneralError;
        break;
      }
      msg->script_id = param2;
      if ( !recv_ptp_data(data,msg->data,msg->size) )
      {
        ptp.code = PTP_RC_GeneralError;
        free(msg);
        break;
      }
      if( !enqueue_script_msg(&msg_q_in,msg) ) {
        ptp.code = PTP_RC_GeneralError;
        free(msg);
      }
      break;
    }

    case PTP_CHDK_GetDisplayData:
        {
            extern int live_view_get_data(ptp_data *data, int flags);

            ptp.num_param = 1;
            ptp.param1 = live_view_get_data(data,param2);
            if(!ptp.param1)
            {
                ptp.code = PTP_RC_GeneralError;
                // send dummy data, otherwise error hoses connection
                send_ptp_data(data,"\0",1);
            }
        }
        break;
    case PTP_CHDK_RemoteCaptureIsReady:
        ptp.num_param = 2;
        remotecap_is_ready(&ptp.param1,&ptp.param2);
        break;
    case PTP_CHDK_RemoteCaptureGetData:
        {
            unsigned int rcgd_size;
            int rcgd_status;
            char *rcgd_addr;
            int rcgd_pos;

            rcgd_status = remotecap_get_data_chunk(param2, &rcgd_addr, &rcgd_size, &rcgd_pos);
            ptp.num_param = 3;
            ptp.param3 = rcgd_pos; //client needs to seek to this file position before writing the chunk (-1 = ignore)
            if ( (rcgd_addr==0) || (rcgd_size==0) ) {
                // send dummy data, otherwise error hoses connection
                send_ptp_data(data,"\0",1);
                ptp.param1 = 0; //size
                ptp.param2 = 0; //0 = no more chunks
            } else {
                // send directly using send_data to avoid multiple send calls
                data->send_data(data->handle,rcgd_addr,rcgd_size,rcgd_size,0,0,0);

                ptp.param1 = rcgd_size; //size
                if(rcgd_status == REMOTECAP_CHUNK_STATUS_MORE) {
                    ptp.param2 = 1;
                } else {
                    ptp.param2 = 0;
                }
            }
            // data send complete, free hooks etc as needed, set error status if required
            if(!remotecap_send_complete(rcgd_status,param2)) {
                ptp.code = PTP_RC_GeneralError;
            }
        }
        break;
    default:
      ptp.code = PTP_RC_ParameterNotSupported;
      break;
  }

  // send response
  data->send_resp( data->handle, &ptp, 0 );

  return 1;
}
