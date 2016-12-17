#include <drpc.h>
#include "mem.h"
#include "channel.h"
#include "session.h"

static void do_session(drpc_session_t sess);

drpc_session_t drpc_session_new(drpc_channel_t chan) {
    drpc_session_t sess = (drpc_session_t)drpc_alloc(sizeof(*sess));
    if (!sess) {
        return NULL;
    }
    sess->execute = (drpc_task_func)do_session;
    sess->channel = chan;
    sess->input = NULL;
    sess->output = NULL;
    return sess;
}

void do_session(drpc_session_t sess) {
    drpc_message_t rmsg = sess->input;
    //fprintf(stderr, "seq=%u payload=%u\n", rmsg->header.sequence, rmsg->header.payload);
    //fflush(stderr);
    //write(STDERR_FILENO, rmsg->body, rmsg->header.payload);
    //write(STDERR_FILENO, "\n", 1);
    drpc_message_t smsg = (drpc_message_t)drpc_alloc(sizeof(*smsg));
    // FIXME check fail
    sess->output = rmsg;
    smsg->header.sequence = rmsg->header.sequence;
    DRPC_LOG(NOTICE, "session process [sequence=%x]", rmsg->header.sequence);

    smsg->header.payload = rmsg->header.payload;
    smsg->body = drpc_alloc(rmsg->header.payload); // ECHO
    memcpy(smsg->body, rmsg->body, rmsg->header.payload); // ECHO

    drpc_free(rmsg->body);
    drpc_free(rmsg);
    drpc_session_drop(sess);
    drpc_channel_send(sess->channel, smsg);
}

void drpc_session_drop(drpc_session_t sess) {
    if (!sess) {
        return;
    }
    drpc_free(sess);
}

