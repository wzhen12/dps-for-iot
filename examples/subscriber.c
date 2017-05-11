/*
 *******************************************************************
 *
 * Copyright 2016 Intel Corporation All rights reserved.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <dps/dbg.h>
#include <dps/dps.h>
#include <dps/synchronous.h>
#include <dps/event.h>

static int quiet = DPS_FALSE;

static uint8_t AckFmt[] = "This is an ACK from %d";

#define NUM_KEYS 2

static DPS_UUID keyId[NUM_KEYS] = { 
    { .val = { 0xed,0x54,0x14,0xa8,0x5c,0x4d,0x4d,0x15,0xb6,0x9f,0x0e,0x99,0x8a,0xb1,0x71,0xf2 } },
    { .val = { 0x53,0x4d,0x2a,0x4b,0x98,0x76,0x1f,0x25,0x6b,0x78,0x3c,0xc2,0xf8,0x12,0x90,0xcc } }
};

/*
 * Preshared keys for testing only - DO NOT USE THESE KEYS IN A REAL APPLICATION!!!!
 */
static uint8_t keyData[NUM_KEYS][16] = {
    { 0x77,0x58,0x22,0xfc,0x3d,0xef,0x48,0x88,0x91,0x25,0x78,0xd0,0xe2,0x74,0x5c,0x10 },
    { 0x39,0x12,0x3e,0x7f,0x21,0xbc,0xa3,0x26,0x4e,0x6f,0x3a,0x21,0xa4,0xf1,0xb5,0x98 }
};

static void OnNodeDestroyed(DPS_Node* node, void* data)
{
    if (data) {
        DPS_SignalEvent((DPS_Event*)data, DPS_OK);
    }
}

static void OnPubMatch(DPS_Subscription* sub, const DPS_Publication* pub, uint8_t* data, size_t len)
{
    DPS_Status ret;
    const DPS_UUID* pubId = DPS_PublicationGetUUID(pub);
    uint32_t sn = DPS_PublicationGetSequenceNum(pub);
    size_t i;
    size_t numTopics;
    int matched = 0;
    int topics = 0;

    if (!quiet) {
        DPS_PRINT("Pub %s(%d) matches:\n", DPS_UUIDToString(pubId), sn);
        DPS_PRINT("  pub ");
        numTopics = DPS_PublicationGetNumTopics(pub);
        for (i = 0; i < numTopics; ++i) {
            if (i) {
                DPS_PRINT(" | ");
            }
            DPS_PRINT("%s", DPS_PublicationGetTopic(pub, i));
        }
        DPS_PRINT("\n");
        DPS_PRINT("  sub ");
        numTopics = DPS_SubscriptionGetNumTopics(sub);
        for (i = 0; i < numTopics; ++i) {
            if (i) {
                DPS_PRINT(" & ");
            }
            if (strstr(DPS_SubscriptionGetTopic(sub, i), "270") != NULL) {
               topics = 1;
            } else if (strstr(DPS_SubscriptionGetTopic(sub, i), "272") != NULL) {
               topics = 2;
            } else if (strstr(DPS_SubscriptionGetTopic(sub, i), "12") != NULL) {
               topics = 3;
            } else if (strstr(DPS_SubscriptionGetTopic(sub, i), "6") != NULL) {
               topics = 4;
            }
            DPS_PRINT("%s", DPS_SubscriptionGetTopic(sub, i));
        }
        DPS_PRINT("\n");
        if (data) {
            int ret = 0;
            if (strcmp(data, "offline") == 0) {
                matched = 1;
            } else if (strcmp(data, "lightoff") == 0 && topics == 1) {
                ret = system("./sensor/joule/lightoff_270");
            } else if (strcmp(data, "lighton") == 0 && topics == 1) {
                ret = system("./sensor/joule/lighton_270");
            } else if (strcmp(data, "lightoff") == 0 && topics == 2) {
                ret = system("./sensor/joule/lightoff_272");
            } else if (strcmp(data, "lighton") == 0 && topics == 2) {
                ret = system("./sensor/joule/lighton_272");
            } else if (strcmp(data, "lightoff") == 0 && topics == 3) {
                ret = system("./sensor/galileo/lightoff_12");
            } else if (strcmp(data, "lighton") == 0 && topics == 3) {
                ret = system("./sensor/galileo/lighton_12");
            } else if (strcmp(data, "lightoff") == 0 && topics == 4) {
                ret = system("./sensor/galileo/lightoff_6");
            } else if (strcmp(data, "lighton") == 0 && topics == 4) {
                ret = system("./sensor/galileo/lighton_6");
            }

            if (ret == -1) {
                DPS_PRINT("run light on/off error\n");
            }
            DPS_PRINT("%.*s\n", (int)len, data);
        }
    }
    if (DPS_PublicationIsAckRequested(pub)) {
        char ackMsg[sizeof(AckFmt) + 8];

        sprintf(ackMsg, AckFmt, DPS_GetPortNumber(DPS_PublicationGetNode(pub)));

        ret = DPS_AckPublication(pub, ackMsg, sizeof(ackMsg));
        if (ret != DPS_OK) {
            DPS_PRINT("Failed to ack pub %s\n", DPS_ErrTxt(ret));
        }
    }

    if (matched == 1) {
        exit(0);
    }
}

static int IntArg(char* opt, char*** argp, int* argcp, int* val, int min, int max)
{
    char* p;
    char** arg = *argp;
    int argc = *argcp;

    if (strcmp(*arg++, opt) != 0) {
        return 0;
    }
    if (!--argc) {
        return 0;
    }
    *val = strtol(*arg++, &p, 10);
    if (*p) {
        return 0;
    }
    if (*val < min || *val > max) {
        DPS_PRINT("Value for option %s must be in range %d..%d\n", opt, min, max);
        return 0;
    }
    *argp = arg;
    *argcp = argc;
    return 1;
}

#define MAX_LINKS 16

int main(int argc, char** argv)
{
    DPS_Status ret;
    char* topicList[64];
    char** arg = argv + 1;
    int numTopics = 0;
    int wait = 0;
    DPS_MemoryKeyStore* memoryKeyStore = NULL;
    const DPS_UUID* nodeKeyId = NULL;
    DPS_Node* node;
    DPS_Event* nodeDestroyed;
    int mcastPub = DPS_MCAST_PUB_DISABLED;
    const char* host = NULL;
    int encrypt = DPS_TRUE;
    int listenPort = 0;
    int numLinks = 0;
    int linkPort[MAX_LINKS];
    const char* linkHosts[MAX_LINKS];

    DPS_Debug = 0;

    while (--argc) {
        /*
         * Topics must come last
         */
        if (numTopics == 0) {
            if (IntArg("-l", &arg, &argc, &listenPort, 1, UINT16_MAX)) {
                continue;
            }
            if (IntArg("-p", &arg, &argc, &linkPort[numLinks], 1, UINT16_MAX)) {
                if (numLinks == (MAX_LINKS - 1)) {
                    DPS_PRINT("Too many -p options\n");
                    goto Usage;
                }
                linkHosts[numLinks] = host;
                ++numLinks;
                continue;
            }
            if (strcmp(*arg, "-h") == 0) {
                ++arg;
                if (!--argc) {
                    goto Usage;
                }
                host = *arg++;
                continue;
            }
            if (strcmp(*arg, "-q") == 0) {
                ++arg;
                quiet = DPS_TRUE;
                continue;
            }
            if (IntArg("-w", &arg, &argc, &wait, 0, 30)) {
                continue;
            }
            if (IntArg("-x", &arg, &argc, &encrypt, 0, 1)) {
                continue;
            }
            if (strcmp(*arg, "-m") == 0) {
                ++arg;
                mcastPub = DPS_MCAST_PUB_ENABLE_RECV;
                continue;
            }
            if (strcmp(*arg, "-d") == 0) {
                ++arg;
                DPS_Debug = 1;
                continue;
            }
        }
        if (strcmp(*arg, "-s") == 0) {
            ++arg;
            /*
             * NULL separator between topic lists
             */
            if (numTopics > 0) {
                topicList[numTopics++] = NULL;
            }
            continue;
        }
        if (*arg[0] == '-') {
            goto Usage;
        }
        if (numTopics == A_SIZEOF(topicList)) {
            DPS_PRINT("%s: Too many topics - increase limit and recompile\n", argv[0]);
            goto Usage;
        }
        topicList[numTopics++] = *arg++;
    }

    if (!numLinks) {
        mcastPub = DPS_MCAST_PUB_ENABLE_RECV;
    }
    if (encrypt) {
        memoryKeyStore = DPS_CreateMemoryKeyStore();
        for (size_t i = 0; i < NUM_KEYS; ++i) {
            DPS_SetContentKey(memoryKeyStore, &keyId[i], keyData[i], 16);
        }
        nodeKeyId = &keyId[0];
    }
    node = DPS_CreateNode("/.", DPS_MemoryKeyStoreHandle(memoryKeyStore), nodeKeyId);

    ret = DPS_StartNode(node, mcastPub, listenPort);
    if (ret != DPS_OK) {
        DPS_ERRPRINT("Failed to start node: %s\n", DPS_ErrTxt(ret));
        return 1;
    }
    DPS_PRINT("Subscriber is listening on port %d\n", DPS_GetPortNumber(node));

    nodeDestroyed = DPS_CreateEvent();

    if (wait) {
        /*
         * Wait for a while before trying to link
         */
        DPS_TimedWaitForEvent(nodeDestroyed, wait * 1000);
    }

    if (numTopics > 0) {
        char** topics = topicList;
        while (numTopics >= 0) {
            DPS_Subscription* subscription;
            int count = 0;
            while (count < numTopics) {
                if (!topics[count]) {
                    break;
                }
                ++count;
            }
            subscription = DPS_CreateSubscription(node, (const char**)topics, count);
            ret = DPS_Subscribe(subscription, OnPubMatch);
            if (ret != DPS_OK) {
                break;
            }
            topics += count + 1;
            numTopics -= count + 1;
        }
        if (ret != DPS_OK) {
            DPS_ERRPRINT("Failed to susbscribe topics - error=%s\n", DPS_ErrTxt(ret));
            DPS_DestroyNode(node, OnNodeDestroyed, nodeDestroyed);
            DPS_WaitForEvent(nodeDestroyed);
            DPS_DestroyEvent(nodeDestroyed);
            return 1;
        }
    }
    if (numLinks) {
        int i;
        DPS_NodeAddress* addr = DPS_CreateAddress();
        for (i = 0; i < numLinks; ++i) {
            ret = DPS_LinkTo(node, linkHosts[i], linkPort[i], addr);
            if (ret != DPS_OK) {
                DPS_ERRPRINT("DPS_LinkTo %d returned %s\n", linkPort[i], DPS_ErrTxt(ret));
                DPS_DestroyNode(node, OnNodeDestroyed, nodeDestroyed);
                break;
            }
        }
        DPS_DestroyAddress(addr);
    }
    DPS_WaitForEvent(nodeDestroyed);
    DPS_DestroyEvent(nodeDestroyed);
    DPS_DestroyMemoryKeyStore(memoryKeyStore);
    return 0;

Usage:
    DPS_PRINT("Usage %s [-d] [-q] [-m] [-w <seconds>] [-x 0/1] [[-h <hostname>] -p <portnum>] [-l <listen port] [-m] [-d] [[-s] topic1 ... topicN]\n", argv[0]);
    DPS_PRINT("       -d: Enable debug ouput if built for debug.\n");
    DPS_PRINT("       -q: Quiet - suppresses output about received publications.\n");
    DPS_PRINT("       -x: Enable or disable encryption. Default is encryption enabled.\n");
    DPS_PRINT("       -h: Specifies host (localhost is default). Mutiple -h options are permitted.\n");
    DPS_PRINT("       -w: Time to wait before establishing links\n");
    DPS_PRINT("       -p: A port to link. Multiple -p options are permitted.\n");
    DPS_PRINT("       -m: Enable multicast receive. Enabled by default is there are no -p options.\n");
    DPS_PRINT("       -l: port to listen on. Default is an ephemeral port.\n");
    DPS_PRINT("       -s: list of subscription topic strings. Multiple -s options are permitted\n");
    return 1;
}
