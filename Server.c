/*
 * Server.c
 *
 *  Created on: May 3, 2013
 *      Author:  Ditu Alexandru
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#define MAX_CLIENTS 10
#define BUFFLEN 1024
#define NAME_LEN 255

typedef struct {
	in_port_t port;
	clock_t start_time;
	struct in_addr ip_addr;
	int status; //1=connected, 0=free slot
	char name[NAME_LEN];
} ClientInfo;



/* aceasta afiseaza la stdin informatiile depsre toti clientii conectati */
void status (ClientInfo *clientInfo, int size) {
	int i;
	char *ip;
	for (i = 0; i < size; i++) {
		if (clientInfo[i].status == 2) {
			printf ("%s %s %d\n", clientInfo[i].name,
					inet_ntoa (clientInfo[i].ip_addr),
					htons(clientInfo[i].port));
		}
	}

}

/* intoarce cati clienti sunt conecati la server */
int getActiveClients (ClientInfo *clientInfo) {
	int nr = 0, i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (clientInfo[i].status == 2) {
			nr ++;
		}
	}
	return nr;
}




/*
*	Utilizare: ./server server_port nume_fisier
*/
int main (int argc, char **argv) {
	int server_sfd, newsockfd, port_no;
	struct sockaddr_in server_addr;
	int n, i, j;
	char buffer [BUFFLEN];


	/* Date pentru max 10 clienti */
	ClientInfo clientInfo[MAX_CLIENTS];
	struct sockaddr_in client_addr[MAX_CLIENTS];
	int client_sfd[MAX_CLIENTS];
	int no_clients = 0; //nr de clienti pentru care s-a facut accept/conectati

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//multime folosita temporar
	int fdmax;		//valoare maxima file descriptor din multimea read_fds



	if (argc < 2) {
		 fprintf(stderr,"Usage : %s port\n", argv[0]);
		 exit(1);
	 }

	 //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
	 FD_ZERO(&read_fds);
	 FD_ZERO(&tmp_fds);

	 server_sfd = socket(AF_INET, SOCK_STREAM, 0);
	 if (server_sfd < 0) {
		error("ERROR opening socket");
	 }

	 port_no = atoi(argv[1]);

	 memset((char *) &server_addr, 0, sizeof(server_addr));
	 server_addr.sin_family = AF_INET;
	 server_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	 server_addr.sin_port = htons(port_no);

	 if (bind(server_sfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) < 0) {
		 error("ERROR on binding");
	 }


	 listen(server_sfd, MAX_CLIENTS);

	 /* adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	  * adaug si file descriptorul pt stdin
	  */
	 FD_SET(server_sfd, &read_fds);
	 FD_SET (STDIN_FILENO, &read_fds);
	 fdmax = server_sfd;

	 int count;

	 printf ("Server started.\n");

	 // main loop
	 while (1) {
		 tmp_fds = read_fds;
		 if (select (fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
			 error ("ERROR in select");
		 }

		 for(i = 0; i <= fdmax; i++) {
			 if (FD_ISSET(i, &tmp_fds)) {

				 /* comanda pentru server */
				 if (i == 0) {
					 memset (buffer, 0, BUFFLEN);
					 fgets(buffer, BUFFLEN, stdin);
					 char *token;
					 token = strtok (buffer, " \n");

					 if (strcmp (token, "status") == 0) {
						 status (clientInfo, MAX_CLIENTS);
					 } else {
						 if (strcmp (token, "kick") == 0) {

							 token = strtok (NULL, " \n");
							 int cl_sfd, k;
							 for (k = 0; k < MAX_CLIENTS; k++) {
								 if (strcmp (clientInfo[k].name, token) == 0) {
									 cl_sfd = client_sfd[k];
									 clientInfo[k].status = 0;
									 break;
								 }
							 }
							 int type = 1;
							 send (cl_sfd, &type, sizeof(int), 0);
							 /* elimin clientul din sistem */
							 FD_CLR (cl_sfd, &read_fds);


						 } else {
							 if (strcmp (token, "quit") == 0) {
								 close (server_sfd);
								 return -1;
							 } else {
								 printf ("Error: enter only status, kick or quit\n");
							 }
						 }
					 }
				 } else {
					/* a venit ceva pe socketul inactiv (cel cu listen) = o noua conexiune
					 * actiunea serverului: accept()
					 */
					if (i == server_sfd) {

						if (no_clients >= MAX_CLIENTS) {
							error ("Error in accept. Maximum clients reched!");
						}else {
							/* ieterz prin lista cu datele clientilor, si caut un slot liber */
							for (j = 0; j < MAX_CLIENTS; j++) {
								if (clientInfo[j].status == 0) {
									int client_len = sizeof (client_addr[j]);
									newsockfd = accept (server_sfd, (struct sockaddr *) &client_addr[j], &client_len);
									break;
									/* actualizare informatii clienti */
								}
							}

							if (newsockfd == -1) {
								error("ERROR in accept");
							} else {
								//adaug noul socket intors de accept() la multimea descriptorilor de citire
								FD_SET(newsockfd, &read_fds);
								client_sfd[j] = newsockfd;
								no_clients ++;

								clientInfo[j].start_time = clock();
								clientInfo[j].status = 1;//connected

								if (newsockfd > fdmax) {
									fdmax = newsockfd;
								}
							}
						}
					} else {
						/* am primit date pe unul din socketii cu care vorbesc cu clientii
						 * actiunea serverului: recv()
						 */

						memset(buffer, 0, BUFFLEN);
						if ((n = recv(i, buffer, BUFFLEN, 0)) <= 0) {
							if (n == 0) {
								//conexiunea s-a inchis
								printf("selectserver: socket %d hung up\n", i);
								int k;
								for (k = 0; k < MAX_CLIENTS; k++) {
									if (client_sfd[k] == i) {
										memset (&clientInfo[k], 0, sizeof(ClientInfo));
										break;
									}
								}
							} else {
								error("ERROR in recv");
							}
							close(i);
							FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care

						} else { //recv intoarce >0

							int type;
							memcpy (&type, buffer, sizeof (int));

							/* type 3: client data
							 * clientul imi trimite datele de autentificare
							 * nume, port, ip */
							if (type == 3) {
								char name[255];
								int depl = sizeof (int);
								memcpy (name, buffer + depl, NAME_LEN);
								depl += NAME_LEN;
								memcpy (&clientInfo[j].port, buffer + depl, sizeof (in_port_t));
								depl += sizeof (in_port_t);
								memcpy (&clientInfo[j].ip_addr, buffer + depl, sizeof (struct in_addr));
								time (&clientInfo[j].start_time);

								int k;
								int ack;
								/* iterez prin lista cu datele clientilor, si
								 * verific daca mai exista un client cu acest
								 * nume;
								 * status = 2 => client conectat, cu nume atasat
								 */
								int duplicate = 0;
								for (k = 0; k < MAX_CLIENTS; k++) {
									if (clientInfo[k].status == 2) {
										if (strcmp (clientInfo[k].name, name) == 0) {
											printf ("Error: Mai exista deja un client cu acest nume\n");
											clientInfo[j].status = 0;
											FD_CLR(newsockfd, &read_fds);
											duplicate = 1;
											ack = -1;
											send (i, &ack, sizeof (int), 0);
											break;
										}
									}
								}
								if (duplicate == 0) {
									memcpy (clientInfo[j].name, name, strlen(name));
									clientInfo[j].status = 2;
									printf ("%s connected succesfuly!\n", clientInfo[j].name);
									ack = 1;
									send (i, &ack, sizeof (int), 0);
								}
							}

							/* type = 1 : cerere lista clienti activi */
							if (type == 1) {
								int nr = getActiveClients (clientInfo);
								send (i, &nr, sizeof(int), 0);
								int k;
								for (k = 0; k < MAX_CLIENTS; k++) {
									if (clientInfo[k].status == 2) {
										send (i, clientInfo[k].name, NAME_LEN, 0);
									}
								}
							}

							/* type = 2: date despre un client */
							if (type == 2) {

								char cl_name[NAME_LEN];
								int k, depl;
								clock_t t;
								int found = 0;

								memcpy (cl_name, buffer + sizeof (int), NAME_LEN);

								for (k = 0; k < MAX_CLIENTS; k++) {
									if (strcmp (clientInfo[k].name, cl_name) == 0 &&
										clientInfo[k].status == 2) {
										found = 1;
										ClientInfo cl_info;
										memcpy (&cl_info, &clientInfo[k], sizeof(ClientInfo));
										time (&t);
										int sec = difftime (t, clientInfo[k].start_time);
										cl_info.start_time = sec;

										memset(buffer, 0, BUFFLEN);
										memcpy (buffer, &found, sizeof(int));
										memcpy (buffer + sizeof(int), &cl_info, sizeof (ClientInfo));
										break;
									}
								}
								if (found != 0) {
									/* daca exista clientul cautat */
									send (i, buffer, BUFFLEN, 0);
								} else {
									memset(buffer, 0, BUFFLEN);
									memcpy (buffer, &found, sizeof (int));
									send (i, buffer, BUFFLEN, 0);
									printf ("Nu exista clientul cautat\n");
								}

							}

							/* type = 4: quit. Un client anunta ca a iesit */
							if (type == 4) {
								close(i);
								FD_CLR(i, &read_fds);
								int k;
								for (k = 0; k < MAX_CLIENTS; k++) {
									if (client_sfd[k] == i) {
										clientInfo[k].status = 0;
										printf ("Client: %s exited!\n",
												clientInfo[k].name);
										break;
									}
								}
							}

						}
					}

				 }
			 }
		 }
	 }

	close (server_sfd);
	return 0;
}
