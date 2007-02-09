/*
 * ClusterDiscoveryService.h
 *
 * Copyright (C) 2006 - 2007 by Universitaet Stuttgart (VIS). 
 * Alle Rechte vorbehalten.
 */

#ifndef VISLIB_CLUSTERDISCOVERYSERVICE_H_INCLUDED
#define VISLIB_CLUSTERDISCOVERYSERVICE_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#include "vislib/IPAddress.h"   // Must be included at begin!
#include "vislib/Socket.h"      // Must be included at begin!
#include "vislib/Array.h"
#include "vislib/ClusterDiscoveryListener.h"
#include "vislib/CriticalSection.h"
#include "vislib/SingleLinkedList.h"
#include "vislib/String.h"
#include "vislib/Thread.h"
#include "vislib/types.h"



namespace vislib {
namespace net {

    /**
     * This class implements a method for discovering other computers in a
     * network via UDP broadcasts. 
     *
     * The user specifies a name as identifier of the cluster to be searched
     * and the object creates an array of all nodes that respond to a request
     * whether they are also members of this cluster. The object also anwers
     * requests of other nodes.
     */
    class ClusterDiscoveryService {

    public:

        /** The default port number used by the discovery service. */
        static const USHORT DEFAULT_PORT;

        /**
         * The maximum size of user data that can be sent via the cluster
         * discovery service in bytes.
         */
        static const SIZE_T MAX_USER_DATA = 256;

        /** 
         * The maximum length of a cluster name in characters, including the
         * trailing zero. 
         */
        static const SIZE_T MAX_NAME_LEN = MAX_USER_DATA 
            - sizeof(struct sockaddr_in);

        /** The first message ID that can be used for a user message. */
        static const UINT16 MSG_TYPE_USER;

        /**
         * Create a new instance.
         *
         * @param name                This is the name of the cluster to 
         *                            detect. It is used to ensure that nodes
         *                            answering a discovery request want to 
         *                            join the same cluster. The name must have 
         *                            at most MAX_NAME_LEN characters.
         * @param responseAddr        This is the "call back address" of the 
         *                            current node, on which user-defined 
         *                            communication should be initiated. The 
         *                            ClusterDiscoveryService does not use this 
         *                            address itself, but justcommunicates it to
         *                            all other nodes which then can use it. 
         *                            These addresses should uniquely identify
         *                            each process in the cluster, i. e. no node
         *                            should specify the same 'responseAddr' as
         *                            some other does.
         * @param bcastAddr           The broadcast address of the network. All
         *                            requests will be sent to this address. The
         *                            destination port of messages is derived 
         *                            from 'bindAddr'. 
         *                            You can use the NetworkInformation to 
         *                            obtain the broadcast address of your 
         *                            subnet.
         * @param bindPort            The port to bind the receiver thread to.
         *                            All discovery requests must be directed to
         *                            this port.
         * @param requestInterval     The interval between two discovery 
         *                            requests in milliseconds.
         * @param cntResponseChances  The number of requests that another node 
         *                            may not answer before being removed from
         *                            this nodes list of known peers.
         *                            operations must timeout.
         */
        ClusterDiscoveryService(const StringA& name, 
            const SocketAddress& responseAddr, 
            const IPAddress& bcastAddr,
            const USHORT bindPort = DEFAULT_PORT,
            const UINT requestInterval = 10 * 1000,
            const UINT cntResponseChances = 1);

        /** 
         * Dtor.
         *
         * Note that the dtor will terminate the discovery.
         */
        virtual ~ClusterDiscoveryService(void);

        /**
         * Add a new ClusterDiscoveryListener to be informed about discovery
         * events. The caller remains owner of the memory designated by 
         * 'listener' and must ensure that the object exists as long as the
         * listener is registered.
         *
         * @param listener The listener to register. This must not be NULL.
         */
        void AddListener(ClusterDiscoveryListener *listener);

        /**
         * Answer the number of known peer nodes. This number includes also
         * this node.
         *
         * @return The number of known peer nodes.
         */
        inline SIZE_T CountPeers(void) const {
            this->critSect.Lock();
            SIZE_T retval = this->peerNodes.Count();
            this->critSect.Unlock();
            return retval;
        }

        /**
         * Answer the address the service is listening on for discovery
         * requests.
         *
         * @return The address the listening socket is bound to.
         */
        const SocketAddress& GetBindAddr(void) const {
            return this->bindAddr;
        }

        /**
         * Answer the cluster identifier that is used for discovery.
         *
         * @return The name.
         */
        inline const StringA& GetName(void) const {
            return this->name;
        }

        /**
         * Answer the call back socket address that is sent to peer nodes 
         * when they are discovered. This address can be used to establish a
         * connection to our node in a application defined manner.
         *
         * @return The address sent as response.
         */
        const SocketAddress& GetResponseAddr(void) const {
            return this->responseAddr;
        }

        /**
         * Removes, if registered, 'listener' from the list of objects informed
         * about discovery events. The caller remains owner of the memory 
         * designated by 'listener'.
         *
         * @param listener The listener to be removed. Nothing happens, if the
         *                 listener was not registered.
         */
        void RemoveListener(ClusterDiscoveryListener *listener);

        /**
         * Send a user-defined message to all known cluster members. The user
         * message can be an arbitrary sequence of a most MAX_USER_DATA bytes.
         *
         * You must have called Socket::Startup before you can use this method.
         *
         * @param msgType The message type identifier. This must be a 
         *                user-defined value of MSG_TYPE_USER or larger.
         * @param msgBody A pointer to the message body. This must not be NULL.
         * @param msgSize The number of valid bytes is 'msgBody'. This must be
         *                most MAX_USER_DATA. All bytes between 'msgSize' and
         *                MAX_USER_DATA will be zeroed.
         *
         * @return Zero in case of success, the number of communication trials
         *         that failed otherwise.
         *
         * @throws SocketException       If the datagram socket for sending the 
         *                               user message could not be created.
         * @throws IllegalParamException If 'msgType' is below MSG_TYPE_USER,
         *                               or 'msgBody' is a NULL pointer,
         *                               or 'msgSize' > MAX_USER_DATA.
         */
        UINT SendUserMessage(const UINT16 msgType, const void *msgBody, 
            const SIZE_T msgSize);

        /**
         * Send a user-defined message to the node that is identified with the
         * response address 'to'. Note that 'to' is not the address which to the
         * message is acutally sent, but only the node identifier. The user
         * message can be an arbitrary sequence of a most MAX_USER_DATA bytes.
         *
         * You must have called Socket::Startup before you can use this method.
         *
         * @param msgType The message type identifier. This must be a 
         *                user-defined value of MSG_TYPE_USER or larger.
         * @param msgBody A pointer to the message body. This must not be NULL.
         * @param msgSize The number of valid bytes is 'msgBody'. This must be
         *                most MAX_USER_DATA. All bytes between 'msgSize' and
         *                MAX_USER_DATA will be zeroed.
         *
         * @return Zero in case of success, the number of communication trials
         *         that failed otherwise.
         *
         * @throws SocketException       If the datagram socket for sending the 
         *                               user message could not be created.
         * @throws IllegalParamException If 'msgType' is below MSG_TYPE_USER,
         *                               or 'msgBody' is a NULL pointer,
         *                               or 'msgSize' > MAX_USER_DATA.
         */
        UINT SendUserMessage(const SocketAddress& to, const UINT16 msgType,
            const void *msgBody, const SIZE_T msgSize);

        /**
         * Start the discovery service. The service starts broadcasting requests
         * into the network and receiving the messages from other nodes. As long
         * as these threads are running, the node is regarded to be a member of
         * the specified cluster.
         *
         * @throws SystemException If the creation of one or more threads 
         *                         failed.
         * @throws std::bad_alloc  If there is not enough memory for the threads
         *                         available.
         */
        virtual void Start(void);

        /** 
         * Stop the discovery service.
         *
         * This operation stops the broadcaster and the receiver thread. The
         * broadcaster will send a disconnect message before it ends.
         *
         * @return true, if the threads have been terminated without any
         *         problem, false, if a SystemException has been thrown by 
         *         one of the threads.
         */
        virtual bool Stop(void);

        /**
         * Answer the application defined communication address of the 'idx'th
         * peer node.
         *
         * @param idx The index of the node to answer, which must be within 
         *            [0, CountPeers()[.
         *
         * @return The response address of the 'idx'th node.
         *
         * @throws OutOfRangeException If 'idx' is not a valid node index.
         */
        inline SocketAddress operator [](const INT idx) const {
            this->critSect.Lock();
            SocketAddress retval = this->peerNodes[idx].address;
            this->critSect.Unlock();
            return retval;
        }

        /**
         * Answer the application defined communication address of the 'idx'th
         * peer node.
         *
         * @param idx The index of the node to answer, which must be within 
         *            [0, CountPeers()[.
         *
         * @return The response address of the 'idx'th node.
         *
         * @throws OutOfRangeException If 'idx' is not a valid node index.
         */
        inline SocketAddress operator [](const SIZE_T idx) const {
            this->critSect.Lock();
            SocketAddress retval = this->peerNodes[idx].address;
            this->critSect.Unlock();
            return retval;
        }

    protected:

        /**
         * This Runnable is the worker that broadcasts the discovery requests of
         * for a specific ClusterDiscoveryService.
         */
        class Sender : public vislib::sys::Runnable {

        public:

            /**
             * Create a new instance that is working for 'cds'.
             *
             * @param cds The ClusterDiscoveryService that determines the 
             *            communication parameters and receives the peer nodes
             *            that have been detected.
             */
            Sender(ClusterDiscoveryService& cds);

            /** Dtor. */
            virtual ~Sender(void);

            /**
             * Performs the discovery.
             *
             * @param reserved Reserved. Must be NULL.
             *
             * @return 0, if the work was successfully finished, an error code
             *         otherwise.
             */
            virtual DWORD Run(const void *reserved = NULL);

            /**
             * Ask the thread to terminate.
             *
             * @return true.
             */
            virtual bool Terminate(void);

        private:

            /** The discovery service the thread is working for. */
            ClusterDiscoveryService& cds;

            /** Flag for terminating the thread safely. */
            bool isRunning;
        }; /* end class Sender */


        /**
         * This Runnable receives discovery requests from other nodes. User
         * messages are also received by this thread and directed to all
         * registered listeners of the ClusterDiscoveryService.
         */
        class Receiver : public vislib::sys::Runnable {

        public:

            /**
             * Create a new instance answering discovery requests directed to
             * 'cds'.
             *
             * @param cds The ClusterDiscoveryService to work for.
             */
            Receiver(ClusterDiscoveryService& cds);

            /** Dtor. */
            virtual ~Receiver(void);

            /**
             * Answers the discovery requests.
             *
             * @param reserved Reserved. Must be NULL.
             *
             * @return 0, if the work was successfully finished, an error code
             *         otherwise.
             */
            virtual DWORD Run(const void *reserved = NULL);

            /**
             * Ask the thread to terminate.
             *
             * @return true.
             */
            virtual bool Terminate(void);

        private:

            /** The discovery service the thread is working for. */
            ClusterDiscoveryService& cds;

            /** Flag for terminating the thread safely. */
            bool isRunning;
        }; /* end class Receiver */

        /* 
         * 'SenderMessageBody' is used for MSG_TYPE_DISCOVERY_REQUEST and
         * MSG_TYPE_SAYONARA in the sender thread.
         */
        typedef struct SenderBody_t {
            struct sockaddr_in sockAddr;	// Peer address to use.
            char name[MAX_NAME_LEN];		// Name of cluster.	
        } SenderMessageBody;

        /**
         * This structure is sent as message by the discovery service. Only one 
         * type of message is used as we cannot know the order and size of
         * UDP datagrams in advance.
         */
        typedef struct Message_t {
            UINT16 magicNumber;						// Must be MAGIC_NUMBER.
            UINT16 msgType;							// The type identifier.
            union {
                SenderMessageBody senderBody;       // I am here messages.
                struct sockaddr_in responseAddr;    // Resonse peer address.
                BYTE userData[MAX_USER_DATA];		// User defined data.
            };
        } Message;

        /** A shortcut to the listener list type. */
        typedef SingleLinkedList<ClusterDiscoveryListener *> ListenerList;

        /** 
         * This structure is used to identify a peer node that has been found
         * during the discovery process.
         */
        typedef struct PeerNode_t {
            SocketAddress address;          // User communication address (ID).
            SocketAddress discoveryAddr;    // Discovery service address.
            UINT cntResponseChances;        // Implicit disconnect detector.

            inline bool operator ==(const PeerNode_t& rhs) const {
                return (this->address == rhs.address);
            }
        } PeerNode;

        /** Such an array is used for storing the known peer nodes. */
        typedef Array<PeerNode> PeerNodeList;

        /**
         * Add 'node' to the list of known peer nodes. If node is already known,
         * the response chance counter is reset to its original value.
         *
         * This method also fires the node found event by calling OnNodeFound
         * on all registered ClusterDiscoveryListeners.
         *
         * This method is thread-safe.
         *
         * @param node The node to be added.
         */
        void addPeerNode(const PeerNode& node);

        /**
         * Inform all registered ClusterDiscoveryListeners about a a user 
         * message that we received.
         *
         * The method will lookup the response address that is passed to the
         * registered listeners itself. 'sender' must therefore be the UDP
         * address of the discovery service. Only the IPAddress of 'sender'
         * is used to identify the peer node, the port is discarded. The 
         * method accepts a socket address instead of an IPAddress just for
         * convenience.
         *
         * If the sender node was not found in the peer node list, no event
         * is fired!
         *
         * This method is thread-safe.
         *
         * @param sender  The socket address of the message sender.
         * @param msgType The message type.
         * @param msgBody The body of the message.
         */
        void fireUserMessage(const SocketAddress& sender, const UINT16 msgType, 
            const BYTE *msgBody) const;

        /**
         * Answer the index of the peer node that runs its discovery 
         * service on 'addr'. If no such node exists, -1 is returned.
         *
         * This method is NOT thread-safe.
         *
         * @param addr The discovery address to lookup.
         *
         * @return The index of the peer node or -1, if not found.
         */
        INT_PTR peerFromDiscoveryAddr(const IPAddress& addr) const;

        /**
         * Answer the index of the peer node that uses 'addr' as its
         * user defined response address. If no such node exists, -1 is 
         * returned.
         *
         * This method is NOT thread-safe.
         *
         * @param addr The response address to search.
         *
         * @return The index of the peer node or -1, if not found.
         */
        INT_PTR peerFromResponseAddr(const SocketAddress& addr) const;

        /**
         * Prepares the list of known peer nodes for a new request. 
         *
         * To do so, the 'cntResponseChances' of each node is checked whether it
         * is zero. If this condition holds true, the node is removed. 
         * Otherwise, the 'cntResponseChances' is decremented.
         *
         * This method also fires the node lost event by calling OnNodeLost
         * on all registered ClusterDiscoveryListeners.
         *
         * This method is thread-safe.
         */
        void prepareRequest(void);

        /**
         * This method prepares sending a user message. The following 
         * steps are taken.
         *
         * All user input, i. e. 'msgType', 'msgBody' and 'msgSize' is validated
         * and an IllegalParamException is thrown, if 'msgType' is not in the
         * user message range, if 'msgBody' is a NULL pointer of 'msgSize' is 
         * too large.
         *
         * If all user input is valid, the datagram to be sent is written to
         * 'outMsg'.
         *
         * It is further checked, whether 'this->userMsgSocket' is already 
         * valid. If not, the socket is created.
         *
         * @param outMsg  The Message structure receiving the datagram.
         * @param msgType The message type identifier. This must be a 
         *                user-defined value of MSG_TYPE_USER or larger.
         * @param msgBody A pointer to the message body. This must not be NULL.
         * @param msgSize The number of valid bytes is 'msgBody'. This must be
         *                most MAX_USER_DATA. All bytes between 'msgSize' and
         *                MAX_USER_DATA will be zeroed.
         *
         * @return Zero in case of success, the number of communication trials
         *         that failed otherwise.
         *
         * @throws SocketException       If the datagram socket for sending the 
         *                               user message could not be created.
         * @throws IllegalParamException If 'msgType' is below MSG_TYPE_USER,
         *                               or 'msgBody' is a NULL pointer,
         *                               or 'msgSize' > MAX_USER_DATA.
         */
        void prepareUserMessage(Message& outMsg, const UINT16 msgType,
            const void *msgBody, const SIZE_T msgSize);

        /**
         * Remove the peer node 'node'.
         *
         * This method also fires the node lost event sigaling an explicit
         * remove of the node.
         *
         * @param node The node to be removed.
         */
        void removePeerNode(const PeerNode& node);

        /** The magic number at the begin of each message. */
        static const UINT16 MAGIC_NUMBER;

        /** Message type ID of a discovery request. */
        static const UINT16 MSG_TYPE_IAMHERE;

        /** Message type ID of the explicit disconnect notification. */
        static const UINT16 MSG_TYPE_SAYONARA;

        /** This is the broadcast address to send requests to. */
        SocketAddress bcastAddr;

        /** The address that the response thread binds to. */
        SocketAddress bindAddr;

        /** The address we send in a response message. */
        SocketAddress responseAddr;

        /**
         * Critical section protecting access to the 'peerNodes' array and the
         * 'listeners' list.
         */
        mutable sys::CriticalSection critSect;

        /** 
         * The number of chances a node gets to respond before it is implicitly 
         * disconnected from the cluster.
         */
        UINT cntResponseChances;

        /** The time in milliseconds between two discovery requests. */
        UINT requestInterval;

        /** This list holds the objects to be informed about new nodes. */
        ListenerList listeners;

        /** The name of the cluster this discovery service should form. */
        StringA name;

        /** The worker object of 'requestThread'. */
        Sender *sender;

        /** The worder object of 'responseThread'. */
        Receiver *receiver;

        /** The thread receiving discovery requests. */
        sys::Thread *receiverThread;

        /** The thread performing the node discovery. */
        sys::Thread *senderThread;

        /** This array holds the peer nodes. */
        PeerNodeList peerNodes;

        /** The socket used for sending user messages. */
        Socket userMsgSocket;

        /* Allow threads to access protected methods. */
        friend class Receiver;
        friend class Sender;
    };


} /* end namespace net */
} /* end namespace vislib */

#endif /* VISLIB_CLUSTERDISCOVERYSERVICE_H_INCLUDED */
