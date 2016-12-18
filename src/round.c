#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <drpc.h>
#include "mem.h"
#include "server.h"
#include "session.h"
#include "round.h"

static void do_round(drpc_round_t round);
static void do_not_found(drpc_round_t round);

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

void drpc_round_complete(drpc_round_t round, uint16_t status) {
    DRPC_ENSURE(round, "invalid argument");
    DRPC_LOG(DEBUG, "round complete");
    round->output.header.status = status;
    if (round->input->body) {
        drpc_free(round->input->body);
    }
    drpc_free(round->input);
    round->input = NULL;
    drpc_session_send(round->session, round);
}

void do_round(drpc_round_t round) {
    drpc_request_t input = round->input;
    drpc_response_t output = &round->output;
    output->header.version = 0x0;
    output->header.compress = 0x0;
    output->header.sequence = input->header.sequence;
    output->header.status = DRPC_STATUS_OK;
    output->header.payload = 0;
    output->body = NULL;
    if (round->session->server->stub_func) {
        round->session->server->stub_func(
            round,
            round->session->server->stub_arg,
            (char*)input->body,
            input->header.payload,
            &output->body,
            &output->header.payload
        );
    } else {
        do_not_found(round);
    }
}

void do_not_found(drpc_round_t round) {
    drpc_response_t output = &round->output;
    output->header.payload = 0;
    output->body = NULL;
    drpc_round_complete(round, DRPC_STATUS_NOT_FOUND);
}

