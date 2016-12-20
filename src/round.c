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
    drpc_round_t round = drpc_new(drpc_round);
    if (!round) {
        DRPC_LOG(NOTICE, "malloc fail: %s", strerror(errno));
        return NULL;
    }
    DRPC_TASK_INIT(round, "round", do_round);
    round->session = sess;
    round->request = sess->input;
    memset(&round->response, 0, sizeof(round->response));
    round->response.header.sequence = 100;
    return round;
}

void drpc_round_drop(drpc_round_t round) {
    DRPC_ENSURE(round, "invalid argument");
    if (round->response.body) {
        //drpc_free(round->response.body);
        free(round->response.body);
    }
    drpc_free(round);
}

void drpc_round_complete(drpc_round_t round, uint16_t status) {
    DRPC_ENSURE(round, "invalid argument");
    DRPC_LOG(DEBUG, "round complete");
    round->response.header.status = status;
    if (round->request->body) {
        drpc_free(round->request->body);
    }
    drpc_free(round->request);
    round->request = NULL;
    drpc_session_send(round->session, round);
}

void do_round(drpc_round_t round) {
    drpc_request_t request = round->request;
    drpc_response_t response = &round->response;
    response->header.version = 0x0;
    response->header.compress = 0x0;
    response->header.sequence = request->header.sequence;
    response->header.status = DRPC_STATUS_OK;
    response->header.payload = 0;
    response->body = NULL;
    if (round->session->server->stub_func) {
        round->session->server->stub_func(
            round,
            round->session->server->stub_arg,
            (char*)request->body,
            request->header.payload,
            &response->body,
            &response->header.payload
        );
    } else {
        do_not_found(round);
    }
}

void do_not_found(drpc_round_t round) {
    drpc_response_t response = &round->response;
    response->header.payload = 0;
    response->body = NULL;
    drpc_round_complete(round, DRPC_STATUS_NOT_FOUND);
}

