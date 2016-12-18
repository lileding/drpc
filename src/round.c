#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <drpc.h>
#include "mem.h"
#include "server.h"
#include "session.h"
#include "round.h"

static void do_round(drpc_round_t round);
static void do_process(drpc_round_t round);
static void do_complete(drpc_round_t round);

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
    round->output.header.sequence = 100;
    return round;
}

void drpc_round_drop(drpc_round_t round) {
    DRPC_ENSURE(round, "invalid argument");
    if (round->output.body) {
        drpc_free(round->output.body);
    }
    drpc_free(round);
}

void do_round(drpc_round_t round) {
    drpc_request_t input = round->input;
    drpc_response_t output = &round->output;
    output->header.version = 0x0;
    output->header.compress = 0x0;
    output->header.sequence = input->header.sequence;
    output->header.status = DRPC_STATUS_OK;
    //round->__drpc_task_func = (drpc_task_func)do_complete;
    do_process(round);
}

void do_process(drpc_round_t round) {
    drpc_request_t input = round->input;
    drpc_response_t output = &round->output;
    output->header.payload = input->header.payload;
    output->body = drpc_alloc(output->header.payload);
    if (!output->body) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        output->body = NULL;
        output->header.payload = 0;
    } else {
        strncpy(output->body, input->body, output->header.payload);
    }

    //drpc_thrpool_apply(&round->session->server->pool, (drpc_task_t)round);
    do_complete(round);
}

void do_complete(drpc_round_t round) {
    DRPC_LOG(NOTICE, "round complete");
    if (round->input->body) {
        drpc_free(round->input->body);
    }
    drpc_free(round->input);
    round->input = NULL;
    drpc_session_send(round->session, round);
}

