#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <drpc.h>
#include "mem.h"
#include "session.h"
#include "round.h"

static void do_round(drpc_round_t round);

drpc_round_t drpc_round_new(drpc_session_t sess) {
    drpc_round_t round = (drpc_round_t)drpc_alloc(sizeof(*round));
    if (!round) {
        DRPC_LOG(NOTICE, "malloc fail: %s", strerror(errno));
        return NULL;
    }
    DRPC_TASK_INIT(round, "round", do_round);
    round->session = sess;
    round->input = NULL;
    round->output = NULL;
    return round;
}

void do_round(drpc_round_t round) {
    drpc_message_t input = round->input;
    drpc_message_t output = (drpc_message_t)drpc_alloc(sizeof(*output));
    if (!output) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        abort();
    }
    round->output = output;
    output->header.version = 0x0;
    output->header.compress = 0x0;
    output->header.sequence = input->header.sequence;

    /* process */
    struct timespec rqtp = { 0, 1000000 };
    nanosleep(&rqtp, NULL);
    output->header.payload = input->header.payload;
    output->body = drpc_alloc(output->header.payload);
    if (!output->body) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        abort();
    }
    strncpy(output->body, input->body, output->header.payload);

    /* send */
    output = round->output;
    round->output = NULL;
    drpc_free(round->input->body);
    drpc_free(round->input);
    round->input = NULL;
    drpc_round_drop(round);
    drpc_session_send(round->session, output);
}

void drpc_round_drop(drpc_round_t round) {
    if (!round) {
        return;
    }
    drpc_free(round);
}

