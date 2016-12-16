#include <drpc.h>
#include "mem.h"
#include "channel.h"
#include "session.h"

drpc_session_t drpc_session_new(drpc_channel_t chan) {
    drpc_session_t sess = (drpc_session_t)drpc_alloc(sizeof(*sess));
    if (!sess) {
        return NULL;
    }
    sess->channel = chan;
    sess->output.body = NULL;
    sess->input.body = NULL;
    sess->iov.iov_base = &sess->input.header;
    sess->iov.iov_len = sizeof(sess->input.header);
    sess->is_body = 0;
    return sess;
}

void drpc_session_process(drpc_session_t sess) {
    drpc_message_t rmsg = &sess->input;
    //fprintf(stderr, "seq=%u payload=%u\n", rmsg->header.sequence, rmsg->header.payload);
    //fflush(stderr);
    //write(STDERR_FILENO, rmsg->body, rmsg->header.payload);
    //write(STDERR_FILENO, "\n", 1);
    drpc_message_t smsg = &sess->output;
    smsg->header.payload = rmsg->header.payload;
    smsg->body = drpc_alloc(rmsg->header.payload); // ECHO
    memcpy(smsg->body, rmsg->body, rmsg->header.payload); // ECHO

    drpc_free(sess->input.body);
    sess->input.body = NULL;
    smsg->header.sequence = rmsg->header.sequence;

    drpc_channel_send(sess->channel, sess);
}

void drpc_session_drop(drpc_session_t sess) {
    if (!sess) {
        return;
    }
    if (sess->output.body != NULL) {
        drpc_free(sess->output.body);
    }
    if (sess->input.body != NULL) {
        drpc_free(sess->input.body);
    }
    //DRPC_LOG(DEBUG, "free session %p", sess);
    drpc_free(sess);
}

