/*
 * server.c
 *
 *  Created on: Oct 9, 2012
 *      Author: hmng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "jsonrpc-c.h"

#define PORT 1234  // the port users will be connecting to

struct jrpc_server my_server;

cJSON* say_hello(jrpc_context *ctx, cJSON *params, cJSON *id) {
	return cJSON_CreateString("Hello!");
}

cJSON* add(jrpc_context *ctx, cJSON *params, cJSON *id) {
	cJSON *a = cJSON_GetArrayItem(params, 0);
	cJSON *b = cJSON_GetArrayItem(params, 1);
	return cJSON_CreateNumber(a->valueint + b->valueint);
}

cJSON* notify(jrpc_context *ctx, cJSON *param, cJSON *id) {
	return NULL;
}

cJSON* read_analog(jrpc_context *ctx, cJSON *params, cJSON *id) {
	// 创建返回的JSON对象
	cJSON *result = cJSON_CreateObject();

	// 添加ao1和ao2的数值
	cJSON_AddNumberToObject(result, "ao1", 300.5);
	cJSON_AddNumberToObject(result, "ao2", 1256.9);

	return result;
}

cJSON* exit_server(jrpc_context *ctx, cJSON *params, cJSON *id) {
	jrpc_server_stop(&my_server);
	return cJSON_CreateString("Bye!");
}

int main(void) {
	jrpc_server_init(&my_server, PORT);
	my_server.debug_level = 1;
	jrpc_register_procedure(&my_server, say_hello, "sayHello", NULL);
	jrpc_register_procedure(&my_server, add, "add", NULL);
	jrpc_register_procedure(&my_server, notify, "notify", NULL);
	jrpc_register_procedure(&my_server, read_analog, "read_analog", NULL);
	jrpc_register_procedure(&my_server, exit_server, "exit", NULL);
	jrpc_server_run(&my_server);
	jrpc_server_destroy(&my_server);
	return 0;
}
