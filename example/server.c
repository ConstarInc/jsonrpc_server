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
#include <stdbool.h>
#include <time.h>
#include "jsonrpc-c.h"

#define PORT 8090   // the port users will be connecting to

struct jrpc_server my_server;
ev_timer update_timer;

// 全局变量用于存储模拟量输入值
double ai1 = 0.0;
double ai2 = 0.0;

// 全局变量用于存储离散量输入值
bool di1 = false;
bool di2 = false;

// 全局变量用于存储模拟量和离散量输出值
double ao1 = 300.5;
double ao2 = 1256.9;
bool do1 = true;
bool do2 = false;

// 生成-10000到10000之间的随机浮点数，最多两位小数
double generate_random_analog() {
	// 生成-1000000到1000000之间的整数，然后除以100得到两位小数
	int random_int = (rand() % 2000001) - 1000000; // -1000000 到 1000000
	return random_int / 100.0; // 转换为-10000.00 到 10000.00
}

// 定时器回调函数，每秒更新数值
static void update_values_cb(struct ev_loop *loop, ev_timer *w, int revents) {
	// 更新模拟量输出值（随机数）
	ao1 = generate_random_analog();
	ao2 = generate_random_analog();

	// 更新离散量输出值（取反）
	do1 = !do1;
	do2 = !do2;

	printf("Updated values: ao1=%.2f, ao2=%.2f, do1=%s, do2=%s\n",
	       ao1, ao2, do1 ? "true" : "false", do2 ? "true" : "false");
}

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

	// 添加ao1和ao2的当前数值（由定时器更新的输出）
	cJSON_AddNumberToObject(result, "ao1", ao1);
	cJSON_AddNumberToObject(result, "ao2", ao2);

	// 添加ai1和ai2的当前数值（通过write_analog写入的输入）
	cJSON_AddNumberToObject(result, "ai1", ai1);
	cJSON_AddNumberToObject(result, "ai2", ai2);

	return result;
}

cJSON* write_analog(jrpc_context *ctx, cJSON *params, cJSON *id) {
	// 检查参数是否为数组
	if (!params || !cJSON_IsArray(params)) {
		ctx->error_code = JRPC_INVALID_PARAMS;
		ctx->error_message = strdup("Parameters must be an array");
		return NULL;
	}

	// 检查数组是否包含两个元素
	if (cJSON_GetArraySize(params) != 2) {
		ctx->error_code = JRPC_INVALID_PARAMS;
		ctx->error_message = strdup("Parameters array must contain exactly 2 elements");
		return NULL;
	}

	// 获取数组中的两个数值
	cJSON *value1 = cJSON_GetArrayItem(params, 0);
	cJSON *value2 = cJSON_GetArrayItem(params, 1);

	// 检查参数是否为数字
	if (!value1 || !cJSON_IsNumber(value1) || !value2 || !cJSON_IsNumber(value2)) {
		ctx->error_code = JRPC_INVALID_PARAMS;
		ctx->error_message = strdup("Both parameters must be numbers");
		return NULL;
	}

	// 将数值写入全局变量
	ai1 = value1->valuedouble;
	ai2 = value2->valuedouble;

	// 创建成功响应
	cJSON *result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "status", "success");
	cJSON_AddStringToObject(result, "message", "Analog values written successfully");
	cJSON_AddNumberToObject(result, "ai1", ai1);
	cJSON_AddNumberToObject(result, "ai2", ai2);

	return result;
}

cJSON* read_discrete(jrpc_context *ctx, cJSON *params, cJSON *id) {
	// 创建返回的JSON对象
	cJSON *result = cJSON_CreateObject();

	// 添加do1和do2的当前布尔值（由定时器更新）
	cJSON_AddBoolToObject(result, "do1", do1);
	cJSON_AddBoolToObject(result, "do2", do2);

	// 添加di1和di2的当前值（输入，来自write_discrete写入的值）
	cJSON_AddBoolToObject(result, "di1", di1);
	cJSON_AddBoolToObject(result, "di2", di2);

	return result;
}

cJSON* write_discrete(jrpc_context *ctx, cJSON *params, cJSON *id) {
	// 检查参数是否为数组
	if (!params || !cJSON_IsArray(params)) {
		ctx->error_code = JRPC_INVALID_PARAMS;
		ctx->error_message = strdup("Parameters must be an array");
		return NULL;
	}

	// 检查数组是否包含两个元素
	if (cJSON_GetArraySize(params) != 2) {
		ctx->error_code = JRPC_INVALID_PARAMS;
		ctx->error_message = strdup("Parameters array must contain exactly 2 elements");
		return NULL;
	}

	// 获取数组中的两个数值
	cJSON *value1 = cJSON_GetArrayItem(params, 0);
	cJSON *value2 = cJSON_GetArrayItem(params, 1);

	// 检查参数是否为布尔值
	if (!value1 || (!cJSON_IsBool(value1) && !cJSON_IsNumber(value1)) ||
	    !value2 || (!cJSON_IsBool(value2) && !cJSON_IsNumber(value2))) {
		ctx->error_code = JRPC_INVALID_PARAMS;
		ctx->error_message = strdup("Both parameters must be boolean values or numbers (0/1)");
		return NULL;
	}

	// 将布尔值写入全局变量
	// 支持布尔值和数字（0为false，非0为true）
	if (cJSON_IsBool(value1)) {
		di1 = cJSON_IsTrue(value1);
	} else {
		di1 = (value1->valueint != 0);
	}

	if (cJSON_IsBool(value2)) {
		di2 = cJSON_IsTrue(value2);
	} else {
		di2 = (value2->valueint != 0);
	}

	// 创建成功响应
	cJSON *result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "status", "success");
	cJSON_AddStringToObject(result, "message", "Discrete values written successfully");
	cJSON_AddBoolToObject(result, "di1", di1);
	cJSON_AddBoolToObject(result, "di2", di2);

	return result;
}

cJSON* exit_server(jrpc_context *ctx, cJSON *params, cJSON *id) {
	jrpc_server_stop(&my_server);
	return cJSON_CreateString("Bye!");
}

int main(void) {
	// 初始化随机数种子
	srand(time(NULL));

	printf("Starting JSON-RPC server on port %d\n", PORT);
	jrpc_server_init(&my_server, PORT);
	my_server.debug_level = 1;

	// 注册所有的RPC方法
	jrpc_register_procedure(&my_server, say_hello, "sayHello", NULL);
	jrpc_register_procedure(&my_server, add, "add", NULL);
	jrpc_register_procedure(&my_server, notify, "notify", NULL);
	jrpc_register_procedure(&my_server, read_analog, "read_analog", NULL);
	jrpc_register_procedure(&my_server, write_analog, "write_analog", NULL);
	jrpc_register_procedure(&my_server, read_discrete, "read_discrete", NULL);
	jrpc_register_procedure(&my_server, write_discrete, "write_discrete", NULL);
	jrpc_register_procedure(&my_server, exit_server, "exit", NULL);

	// 初始化并启动定时器，每1秒触发一次
	ev_timer_init(&update_timer, update_values_cb, 1.0, 1.0);
	ev_timer_start(my_server.loop, &update_timer);

	printf("Server started with timer updating values every second...\n");
	jrpc_server_run(&my_server);

	// 停止定时器并清理
	ev_timer_stop(my_server.loop, &update_timer);
	jrpc_server_destroy(&my_server);
	return 0;
}
