#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

/*****************************************************************
 *                         MACROS
 *****************************************************************/
#define MAX_CONNECTIONS 2
#define MAX_BACKLOG     MAX_CONNECTIONS
#define MAX_MSG_SIZE    100
#define TERMINATE_CODE  "0x59"


/*****************************************************************
 *                         STRUCTURES
 *****************************************************************/
typedef struct
{
    int id;
    int socket_fd;
    int port_no;
    struct sockaddr_in addr;
    socklen_t addrLen;
    char addr_in_str[16];
} device_t;


/*****************************************************************
 *                         FUNCTION PROTOTYPES
 *****************************************************************/
void *acceptConnections(void *args);        /* Thread handler: accept a new connection  */
void *receiveFromPeer(void *args);          /* Thread handler: receive data from a peer*/
int connectToPeer(device_t *peer_device);   /* Connect to a new peer */
int sendToPeer(int peer_id, char *mesg);    /* Send a message to a peer */
int terminatePeer(int peer_id);             /* Terminate a peer */
void listPeer(void);                        /* List all peers connected to this process */
void updatePeerList(int peer_id);           /* Helper function: update  the peer list (after create/close a connection)*/
void showHelp(void);                        /* Show user interface options*/
void showIP(void);                          /* Show the computer IP address */
void showPort(void);                        /* Show the port that this process is listening on */
void exitApp(void);                         /* Close all the communications & terminate this process */

/*****************************************************************
 *                         GLOBAL VARIABLES
 *****************************************************************/
device_t this_device = {0};                 /* Struct variable for this device (the process itself) */
device_t peer_device[MAX_CONNECTIONS] = {0};/* Struct variables for peers connected to this process */

int total_devices = 0;                      /* Total devices, which get updated when there is a new connection/disconnection */

pthread_t acceptThread;
pthread_t receiveThread[MAX_CONNECTIONS];
pthread_mutex_t peer_mutex = PTHREAD_MUTEX_INITIALIZER;


/*****************************************************************
 *                         MAIN FUNCTION
 *****************************************************************/
int main (int argc, char *argv[])
{
    char user_cmd[50];
    char temp_buf;

    system("clear");


    /* Initialize this device's socket */
    if (argc < 2)
    {
        printf("Try command: ./chat <port_number>\n");
        exit(EXIT_FAILURE);
    }
    this_device.port_no = atoi(argv[1]);

    this_device.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this_device.socket_fd == -1)
    {
        perror("Error creating this device's socket");
        exit(EXIT_FAILURE);
    }

    this_device.addr.sin_family = AF_INET;
    this_device.addr.sin_port = htons(this_device.port_no);
    this_device.addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(this_device.socket_fd, (struct sockaddr*)&this_device.addr, sizeof(this_device.addr)) == -1)
    {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(this_device.socket_fd, MAX_BACKLOG) == -1)
    {
        perror("Error listenings socket");
        exit(EXIT_FAILURE);
    }

    showHelp();

    printf("Application is listening on port: %d\n", this_device.port_no);

    /* Create the thread that will handle the passive tasks (accept a new connection,...)*/
    if (pthread_create(&acceptThread, NULL, &acceptConnections, NULL) != 0)
    {
        perror("Error creating new thread");
        exit(EXIT_FAILURE);
    }

    sleep(1);

    int ret_val;
    char temp_cmd[20];
    char temp_addr[16];
    int temp_port_no;

    /* This loop will handle the active tasks (execute user command) */
    while(1)
    {
        /* Get user command */
        printf("\nEnter your command: ");
        fgets(user_cmd, 50, stdin);

        /* Analyze the command */
        ret_val = sscanf(user_cmd, "%s", temp_cmd);

        if (!strcmp(temp_cmd, "connect"))
        {
            /* Command: connect to a Peer */ 
            if (total_devices >= MAX_CONNECTIONS)
            {
                printf("No more rooms for a new connection. Please close a current one.\n");
            }
            else
            {
                sscanf(user_cmd, "%s %s %d", temp_cmd, temp_addr, &temp_port_no);
                
                // printf("User command: %s\n", temp_cmd);
                // printf("Peer addr: %s\n", temp_addr);
                // printf("Peer port: %d\n", temp_port_no);

                // Prepare information for the new peer
                pthread_mutex_lock(&peer_mutex);

                if (inet_pton(AF_INET, temp_addr, &peer_device[total_devices].addr.sin_addr) == -1)
                {
                    perror("Error using inet_pton");
                    exit(EXIT_FAILURE);
                }
                
                peer_device[total_devices].port_no = temp_port_no;
                peer_device[total_devices].id = total_devices;
                sprintf(peer_device[total_devices].addr_in_str, "%s", temp_addr);
                
                // Connect to the new peer
                ret_val = connectToPeer(&peer_device[total_devices]);

                if (ret_val == 0)
                {
                    // Create a thread to receive data from the peer
                    if (pthread_create(&receiveThread[total_devices], NULL, &receiveFromPeer, (void*)&peer_device[total_devices]) != 0)
                    {
                        perror("Error creating 'receiveFromPeer' thread");
                        exit(EXIT_FAILURE);
                    }
                    total_devices++;
                    printf("\nConnected successfully. Ready for data transmission\n");
                }
                else if(ret_val == -1)
                {
                    printf("\nAddress or Port number is not available. Try again.\n");
                }             

                pthread_mutex_unlock(&peer_mutex);
            }
        }
        else if (!strcmp(temp_cmd, "send"))
        {
            /* Command: Send a message to a peer */
            int peer_id;
            char peer_id_str[10];
            char *msg;

            sscanf(user_cmd, "%s %d", temp_cmd, &peer_id);
            sprintf(peer_id_str, "%d", peer_id);

            msg = user_cmd + (strlen(temp_cmd) + strlen(peer_id_str) + 2);

            pthread_mutex_lock(&peer_mutex);

            ret_val = sendToPeer(peer_id, msg);

            pthread_mutex_unlock(&peer_mutex);

            if (ret_val == 0)
            {      
                printf("\nSent message successfully.\n");
            }
            else if (ret_val == -1)
            {
                printf("\nNo peer is available for sending.\n");
            }
        }
        else if (!strcmp(temp_cmd, "terminate"))
        {
            /* Command: terminate a peer connection */
            
            int peer_id;
            sscanf(user_cmd, "%s %d", temp_cmd, &peer_id);

            pthread_mutex_lock(&peer_mutex);

            ret_val = terminatePeer(peer_id);

            pthread_mutex_unlock(&peer_mutex);
            
            if (ret_val == 0)
            { 
                printf("\nTerminate peer with ID %d successfully.\n", peer_id);
            }
            else if(ret_val == -1)
            {
                printf("\nNo peer is available for terminating.\n");
            }

        }
        else if (!strcmp(temp_cmd, "list"))
        {
            /* Command: list all the connection connected to this device */ 
            listPeer();
        }
        else if (!strcmp(temp_cmd, "help"))
        {
            /* Command: show user interface options */
            showHelp();
        }
        else if (!strcmp(temp_cmd, "myip"))
        {
            /* Command: show this device's IP address */
            showIP();
        }
        else if (!strcmp(temp_cmd, "myport"))
        {
            /* Command: show this device's listening port */
            showPort();
        }
        else if (!strcmp(temp_cmd, "exit"))
        {
            /* Command: close all connections & terminate this process */
            exitApp();
        }
        else
        {
            printf("\nInvalid command. Try again\n");
        } 
    }

    return 0;
}

/*****************************************************************
 *                      FUNCTION DEFINITIONS
 *****************************************************************/

/*************************************************************************
 * @fn				- acceptConnections
 *
 * @brief			- Thread handler: wait & accept a new connection
 * 
 * @param[in]		- no use
 *
 * @return			- none
 */
void *acceptConnections(void *args)
{
    int new_socket_fd;
    struct sockaddr_in new_addr;
    socklen_t addrLen = sizeof(new_addr);

    while(1)
    {
        new_socket_fd = accept(this_device.socket_fd, (struct sockaddr*)&new_addr, &addrLen);
        if (new_socket_fd == -1)
        {
            perror("Error accepting new connection.");
            exit(EXIT_FAILURE);
        }

        char addr_in_str[16];
        int print_port = ntohs(new_addr.sin_port);
        inet_ntop(AF_INET, &new_addr.sin_addr, addr_in_str, 16);
        
        // Update new connection to the peer list
        pthread_mutex_lock(&peer_mutex);

        peer_device[total_devices].socket_fd = new_socket_fd;
        peer_device[total_devices].id = total_devices;
        peer_device[total_devices].addr = new_addr;
        peer_device[total_devices].addrLen = addrLen;
        peer_device[total_devices].port_no = print_port;
        sprintf(peer_device[total_devices].addr_in_str, "%s", addr_in_str);

        // Create a thread to receive data from this peer
        if (pthread_create(&receiveThread[total_devices], NULL, &receiveFromPeer, (void*)&peer_device[total_devices]) != 0)
        {
            perror("Error creating 'receiveFromPeer' thread");
            exit(EXIT_FAILURE);
        }
        total_devices++;
        pthread_mutex_unlock(&peer_mutex);

        printf("\nAccepted a new connection from adddress: %s, setup at port: %d\n", addr_in_str, print_port);
    }
}

/*************************************************************************
 * @fn				- connectToPeer
 *
 * @brief			- connect to a peer
 * 
 * @param[in]		- peer_device: holds information about the peer
 *
 * @return			- 0 if success, -1 if failed
 */
int connectToPeer(device_t *peer_device)
{
    /* Initialize & connect to peer device */
    peer_device->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_device->socket_fd == -1)
    {
        perror("Error creating new peer socket");
        exit(EXIT_FAILURE);
    }

    peer_device->addr.sin_family = AF_INET;
    peer_device->addr.sin_port = htons(peer_device->port_no);
    
    if (connect(peer_device->socket_fd, \
                (struct sockaddr*)&peer_device->addr, \
                (socklen_t)sizeof(peer_device->addr))== -1)
    {
        perror("Error connecting to new peer socket");
        return -1;       
    }

    return 0;
}

/*************************************************************************
 * @fn				- sendToPeer
 *
 * @brief			- send a message to a peer
 * 
 * @param[in]		- peer_id: id of the peer
 * @param[in]		- mesg: message string
 *
 * @return			- 0 if success, -1 if failed
 */
int sendToPeer(int peer_id, char *mesg)
{
    int i;
    int ret_val;

    /* Check which device has the id */
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (peer_id == peer_device[i].id)
        {
            /* Send data to the peer */
            ret_val = send(peer_device[i].socket_fd, mesg, strlen(mesg), 0);
            if (ret_val == -1)
            {
                perror("Error sending data to peer");
                exit(EXIT_FAILURE);
            }
            return 0;
        }
    }
    return -1;
}

/*************************************************************************
 * @fn				- Thread handler: receiveFromPeer
 *
 * @brief			- Each connection will have a thread to wait & receive a new message.
 * 
 * @param[in]		- args: points to the corresponding peer device structure
 *
 * @return			- none
 */
void *receiveFromPeer(void *args)
{
    device_t *peer_device = (device_t *)args;
    char recv_buffer[MAX_MSG_SIZE];

    int read_bytes;

    while(1)
    {
        read_bytes = recv(peer_device->socket_fd, recv_buffer, sizeof(recv_buffer), 0);

        if (read_bytes <= 0)
        {
            // An error happened - or a peer has closed a connection.
            printf("\nA peer has left your chat.\n");

            pthread_mutex_lock(&peer_mutex);

            // close(peer_device->socket_fd);
            // peer_device->socket_fd = -1;
            // printf("\nID: %d\n", peer_device->id);
            updatePeerList(peer_device->id);

            //terminatePeer(peer_device->id);

            pthread_mutex_unlock(&peer_mutex);
            
            break;
        }
        if (!strcmp(recv_buffer, TERMINATE_CODE))   // Check did the buffer receive the Close request
        {
            close(peer_device->socket_fd);
            printf("\nThe peer at port %d has disconnected.\n", peer_device->port_no);
            updatePeerList(peer_device->id);
            break;
        }

        printf("\n********************************************\n");
        printf("* Message received from: %s\n", peer_device->addr_in_str);
        printf("* Sender's port: %d\n", peer_device->port_no);
        printf("* Message: %s", recv_buffer);
        printf("********************************************\n");
        printf("\nEnter your command: ");

        memset(recv_buffer, 0, sizeof(recv_buffer));
    }

    pthread_exit(NULL);
}


/*************************************************************************
 * @fn				- terminatePeer
 *
 * @brief			- Terminate a peer on the list by sending a terminate code to it
 *                  - After the disconnection from the peer, this program will catch & close the peer's socket
 * 
 * @param[in]		- peer_id: id of the peer
 *
 * @return			- 0 if success, -1 if failed
 */
int terminatePeer(int peer_id)
{
    int i;
    
    for(i = 0; i < total_devices; i++)
    {
        if ((peer_device[i].id == peer_id) && (peer_device[i].port_no != 0))
        {

            //peer_device[peer_id].socket_fd = -99;

            // Np need to close socket here, the receiveFromPeer thread will handle it if the connection is failed.
            sendToPeer(peer_id, TERMINATE_CODE);

            //close(peer_device[peer_id].socket_fd);    // => receiveFromPeer thread will close

            //memset(&peer_device[i], 0, sizeof(peer_device[i]));
            //updatePeerList(peer_id);

            return 0;
        }
    }

    return -1;
}

/*************************************************************************
 * @fn				- updatePeerList
 *
 * @brief			- Update the peer list when there is a new connection or disconnection.
 * 
 * @param[in]		- peer_id: id of the peer
 *
 * @return			- none
 */
void updatePeerList(int peer_id)
{
    int i;
    if (total_devices > 1)
    {
        for (i = peer_id; i < total_devices - 1; i++)
        {
            peer_device[i] = peer_device[i + 1];
            peer_device[i].id -= 1;
            //printf("ID: %d\n", peer_device[i].id);
        }
    }
    else if (total_devices == 1)
    {
        memset(&peer_device[0], 0, sizeof(peer_device[0]));
    }
    total_devices--;
}


/*************************************************************************
 * @fn				- exitApp
 *
 * @brief			- Close all the communications & terminate this process
 * 
 * @param[in]		- 
 *
 * @return			- none
 */
void exitApp(void)
{
    for (int i = 0; i < total_devices; i++)
    {
        if (peer_device[i].port_no != 0)
        {  
           //close(peer_device[i].socket_fd);
           terminatePeer(i);
        }
    }
    close(this_device.socket_fd);
    exit(EXIT_SUCCESS);
}


/*************************************************************************
 * @fn				- listPeer
 *
 * @brief			- list all the peers that connected to this process.
 * 
 * @param[in]		- 
 *
 * @return			- none
 */
void listPeer(void)
{
    printf("\n********************************************\n");
    printf("ID |        IP Address         | Port No.\n");
    for (int i = 0; i < total_devices; i++)
    {
        if (peer_device[i].port_no != 0)
        {
            printf("%d  |     %s       | %d\n", peer_device[i].id, peer_device[i].addr_in_str, peer_device[i].port_no);
        }
    }
    printf("********************************************\n");
}

/*************************************************************************
 * @fn				- showHelp
 *
 * @brief			- show the user interface options 
 * 
 * @param[in]		- 
 *
 * @return			- none
 */
void showHelp(void)
{
    printf("\n******************************** Chat Application ********************************\n");
    printf("\nUse the commands below:\n");
    printf("1. help                             : display user interface options\n");
    printf("2. myip                             : display IP address of this app\n");
    printf("3. myport                           : display listening port of this app\n");
    printf("4. connect <destination> <port no>  : connect to the app of another computer\n");
    printf("5. list                             : list all the connections of this app\n");
    printf("6. terminate <connection id>        : terminate a connection\n");
    printf("7. send <connection id> <message>   : send a message to a connection\n");
    printf("8. exit                             : close all connections & terminate this app\n");  
    printf("\n**********************************************************************************\n");
}

/*************************************************************************
 * @fn				- showIP
 *
 * @brief			- show this computer's IPv4 address
 * 
 * @param[in]		- 
 *
 * @return			- none
 */
void showIP(void)
{
    char ip_address[16];

    FILE *fp = popen("hostname -I", "r");

    fscanf(fp, "%s", ip_address);

    printf("\nIP Address of this app: %s\n", ip_address);
}

/*************************************************************************
 * @fn				- showPort
 *
 * @brief			- show the port that this process is listening on
 * 
 * @param[in]		- 
 *
 * @return			- none
 */
void showPort(void)
{
    printf("\nListening port of this app: %d\n", this_device.port_no);
}