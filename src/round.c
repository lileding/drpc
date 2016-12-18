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
    round->input = sess->input;
    memset(&round->output, 0, sizeof(round->output));
    return round;
}

void do_round(drpc_round_t round) {
    drpc_message_t input = round->input;
    drpc_message_t output = &round->output;
    output->header.version = 0x0;
    output->header.compress = 0x0;
    output->header.sequence = input->header.sequence;

    /* process */
    output->header.payload = input->header.payload;
    output->body = drpc_alloc(output->header.payload);
    if (!output->body) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        output->body = NULL;
        output->header.payload = 0;
    } else {
        strncpy(output->body, input->body, output->header.payload);
    }

    /* send */
    if (round->input->body) {
        drpc_free(round->input->body);
    }
    drpc_free(round->input);
    round->input = NULL;
    drpc_session_send(round->session, round);
}

void drpc_round_drop(drpc_round_t round) {
    DRPC_ENSURE(round, "invalid argument");
    if (round->output.body) {
        drpc_free(round->output.body);
    }
    drpc_free(round);
}

