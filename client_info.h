/*
 * client_info.h
 *
 *  Created on: Jul 4, 2019
 *      Author: zhanlei
 */

#ifndef CLIENT_INFO_H_
#define CLIENT_INFO_H_

typedef struct {
    const char *username;
    const char *password;
}stClient;

enum ClientID {
	e_CID_UC = 0,
	e_CID_SLC,
	e_CID_HMI,
	e_CID_NMC1,
	e_CID_NMC2,
	e_CID_SIM1,
	e_CID_SIM2,
	e_CID_SIM3,
	e_CID_SIM4,
	e_CID_INVALID
};

extern const stClient constant_clients[e_CID_INVALID];


#endif /* CLIENT_INFO_H_ */
