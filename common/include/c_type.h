/*
Copyright 2020 chseasipder

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef __C_TYPE_H__
#define __C_TYPE_H__

typedef signed char __s8;
typedef unsigned char __u8;
typedef signed short __s16;
typedef unsigned short __u16;
typedef signed int __s32;
typedef unsigned int __u32;
typedef signed long long __s64;
typedef unsigned long long __u64;

enum C_NAT_TYPE
{
	UNKNOWN_NAT_TYPE = 0,	// 未知,待检测
	CONE_NAT_TYPE,			// 锥形NAT
	SYMMETRICAL_NAT_TYPE,	// 对称NAT
	NIUBILITY_NAT_TYPE,		// IP是公网IP
};

#endif
