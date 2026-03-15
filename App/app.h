#ifndef __APP_H__
#define __APP_H__

// 应用层初始化 (调用各驱动的Init)
void App_Init(void);

// 应用层主循环 (放在 main 的 while(1) 里)
void App_Task(void);

#endif
