/*
 * Client.c
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

#define NAME_LEN 255
#define BUFFLEN 1024
#define MAX_CLIENTS 10
#define HLEN 50

typedef struct {
	in_port_t port;
	clock_t start_time;
	struct in_addr ip_addr;
	int status; //1=connected, 0=free slot
	char name[NAME_LEN];
} ClientInfo;

typedef struct {
	char timestamp[20];
	char name[NAME_LEN];
	char msg[BUFFLEN];
}HMessage;

typedef struct {
	char from_name[NAME_LEN];
	char file_name[NAME_LEN];
}FMessage;

ClientInfo getInfoClient (char *name, int server_sfd) {
	int type = 2;
	char msg[BUFFLEN];
	ClientInfo cl_info;
	int depl;

	memcpy (msg, &type, sizeof (int));
	memcpy (msg + sizeof (int), name, NAME_LEN);

	send (server_sfd, msg, BUFFLEN, 0);

	memset (msg, 0, BUFFLEN);
	recv (server_sfd, msg, BUFFLEN, 0);
	memcpy (&type, msg, sizeof (int));
	if (type == 1) {
		/* a fost gasit clientul cautat */
		memcpy (&cl_info, msg + sizeof(int), sizeof(ClientInfo));
	} else {
		printf ("Error: Nu s-a gasit clientul cautat!\n");
		cl_info.status = 0;
	}
	return cl_info;


}

void sendMessage (in_port_t port, char name[NAME_LEN], char *msg, int msg_size) {
	struct sockaddr_in cl_addr;
	int to_client;
	int type = 1;
	char buffer[BUFFLEN];
	int count;

	cl_addr.sin_family = AF_INET;
	cl_addr.sin_port = port;
	inet_aton("127.0.0.1", &cl_addr.sin_addr);

	to_client = socket(AF_INET, SOCK_STREAM, 0);

	if (to_client < 0)
		printf ("ERROR to_client\n");

	if (connect (to_client,(struct sockaddr*) &cl_addr, sizeof(cl_addr)) < 0) {
		printf ("Error in connect\n");
	}

	/* trimit mesajul efectiv */
	int depl2;
	memset (buffer, 0, BUFFLEN);
	type = 1;
	memcpy (buffer, &type, sizeof (int));
	depl2 = sizeof (int);
	memcpy (buffer + depl2, name, NAME_LEN);
	depl2 += NAME_LEN;
	memcpy (buffer + depl2, msg, msg_size);


	count = send (to_client, buffer, BUFFLEN, 0);
	close (to_client);
}


/*
*	Utilizare: ./client nume_client port_client ip_server port_server
*/
int main(int argc, char **argv) {



	int server_sfd, my_sfd;
	struct sockaddr_in server_addr, my_addr;
	char buffer[BUFFLEN];

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//multime folosita temporar
	int fdmax;		//valoare maxima file descriptor din multimea read_fds

	/* date pentru trimis fisier */
	int fld, status;
	struct stat f_status;
	int to_client_sfd;
	char to_client_name[NAME_LEN];

	int fld_to_recv;

	char name[NAME_LEN];
	int port_client, port_server;
	struct in_addr ip_server;

	/* vectorul in care retin istoricul mesajelor */
	int length = sizeof (HMessage);
	char history[HLEN][length];
	int h_counter = 0;

	if (argc < 5) {
		printf ("Usage: <nume_client> <port_client> <ip_server> <port_server>\n");
		return -1;
	}

	/* parsere date de intrare */
	strcpy (name, argv[1]);
	port_client = atoi (argv[2]);
	inet_aton (argv[3], &ip_server);
	port_server = atoi (argv[4]);

	/* deschidere socket pe care ascult conexiuni */
	my_sfd = socket (PF_INET, SOCK_STREAM, 0);

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons (port_client);
	my_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(my_sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0) {
		error("ERROR on binding");
	}

	listen (my_sfd, MAX_CLIENTS);



	/*Deschidere socket pentru conectare la server*/
	server_sfd = socket(PF_INET, SOCK_STREAM, 0);

	/*Setare struct sockaddr_in pentru a specifica unde trimit datele*/
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons (port_server);
	server_addr.sin_addr.s_addr = ip_server.s_addr;

	connect (server_sfd, (struct sockaddr *) &server_addr, sizeof (struct sockaddr_in));

	/* Trimit numele clientului, adresa ip si portul
	 * apoi trebuie validat numele
	 * mesajul ce contine aceste date este de tipul 3;
	 */

	int type = 3;
	int depl;
	memcpy (buffer, &type, sizeof (int));
	depl = sizeof (int);
	memcpy (buffer + depl, name, NAME_LEN);
	depl += NAME_LEN;
	memcpy (buffer + depl, &my_addr.sin_port, sizeof (my_addr.sin_port));
	depl += sizeof (my_addr.sin_port);
	struct in_addr my_ip;
	inet_aton ("127.0.0.1", &my_ip);
	memcpy (buffer + depl, &my_ip, sizeof (my_addr.sin_addr));

	int count = send (server_sfd, buffer, BUFFLEN, 0);
	int ack;
	recv (server_sfd, &ack, sizeof (int), 0);
	if (ack == -1) {
		printf ("Error: name already exists. Reconnect with another name\n");
		return -1;
	} else {
		printf ("Connected succesfuly, name accepted\n");
	}

	FD_ZERO (&read_fds);
	FD_ZERO (&tmp_fds);

	FD_SET (my_sfd, &read_fds);
	FD_SET (STDIN_FILENO, &read_fds);
	FD_SET (server_sfd, &read_fds);
	fdmax = server_sfd;


	printf ("Clients's server stared\n");
	int i, j;

	struct timeval timeout;
	timeout.tv_sec = 1;

	while (1) {
		tmp_fds = read_fds;

		/* testez daca este un fisier in curs de trimitere, daca da, mai trimit
		 * o "bucata" din el.
		 * status = 1 -> in curs de trimitere
		 * status = 0 -> altfel
		 */
		if (status == 1) {

			/* verific daca mai este conectat clientul */
			ClientInfo cl_info = getInfoClient (to_client_name, server_sfd);

			if (cl_info.status == 2) {

				char buf[BUFFLEN];
				int count;
				int type = 3;
				int size = BUFFLEN - 2 * sizeof (int);
				count = read (fld, buf + 2 * sizeof (int), size);

				if (count == 0) {
					type = 4; //nu mai am ce citi din fisier
					close (fld);
					status = 0;
					printf ("Done sending file\n");
				}

				memcpy (buf, &type, sizeof (int));
				memcpy (buf + sizeof (int), &count, sizeof (int));

				int n = send (to_client_sfd, buf, BUFFLEN, 0);
			} else {
				printf ("Error client disconected\n");
				close (fld);
				status = 0;
			}
		}

		if (select (fdmax + 1, &tmp_fds, NULL, NULL, &timeout) == -1) {
			error ("ERROR in select");
		}
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {

				/* comanda pentru client de la stdin */
				if (i == 0) {

					memset (buffer, 0, BUFFLEN);
					fgets(buffer, BUFFLEN, stdin);
					int size = (int)strlen (buffer);
					char *token;
					token = strtok (buffer, " \n");

					if (strcmp (token, "listclients") == 0) {
						type = 1;
						int msg_len = sizeof (int) * sizeof (char);
						char *msg = malloc (msg_len);
						memcpy (msg, &type, msg_len);
						/* trimit serverului cerere pentru lista clienti */
						send (server_sfd, msg, msg_len, 0);

						/* acsta imi trimite lista de clienti astfel:
						 * imi trimite nr de clienti conectati
						 * aloc memorie pentru toti
						 * primesc cate un mesaj cu numele fiecaruia
						 */

						int no;
						char **clients_list;
						memset (msg, 0, sizeof(msg));
						count = recv (server_sfd, &no, sizeof(int), 0);

						clients_list = malloc (no * sizeof (char*));
						for (j = 0; j < no; j++) {
							clients_list[j] = malloc (NAME_LEN * sizeof(char));
							recv (server_sfd, clients_list[j], NAME_LEN, 0);
							printf ("%d: %s\n", j, clients_list[j]);
						}

					}
					/* comanda: infoclient <nume_client> */
					if (strcmp (token, "infoclient") == 0) {
						token = strtok (NULL, " \n");
						char msg[BUFFLEN];
						type = 2;
						memcpy (msg, &type, sizeof (int));
						memcpy (msg + sizeof (int), token, strlen (token) + 1);

						send (server_sfd, msg, BUFFLEN, 0);

						memset (msg, 0, BUFFLEN);
						int sz = recv (server_sfd, msg, BUFFLEN, 0);
						memcpy (&type, msg, sizeof (int));

						if (type == 1) {
							/* a fost gasit clientul cautat */
							ClientInfo cl_info;
							memcpy (&cl_info, msg + sizeof(int), sizeof(ClientInfo));
							int sec = cl_info.start_time;

							/* afisare informatii client */
							printf ("[time: %dsec][name: %s][port: %d]\n", sec, cl_info.name, ntohs(cl_info.port));
						} else {
							/* clientul cautat nu exista */
							printf ("Error: Nu s-a gasit clientul cautat!\n");
						}


					}

					/* comanda: message <nume_client> <mesaj> */
					if (strcmp (token, "message") == 0) {

						char cl_name [NAME_LEN];
						token = strtok (NULL, " \n");
						strcpy (cl_name, token);
						char msg [BUFFLEN];
						int depl = 9 + (int) strlen (cl_name);
						memcpy (msg, buffer + depl, size - depl - 1);

						/* Cer serverului informatii despre client:
						 * port si ip
						 */
						type = 2;
						memset (msg, 0, BUFFLEN);
						memcpy (msg, &type, sizeof (int));
						memcpy (msg + sizeof (int), cl_name, NAME_LEN);
						send (server_sfd, msg, BUFFLEN, 0);

						memset (msg, 0, BUFFLEN);
						recv (server_sfd, msg, BUFFLEN, 0);
						memcpy (&type, msg, sizeof (int));

						if (type == 1) {
							/* a fost gasit clientul cautat */
							ClientInfo cl_info;
							memcpy (&cl_info, msg + sizeof(int), sizeof(ClientInfo));
							sendMessage (cl_info.port, name, buffer + depl, size - depl - 1);
						} else {
							printf ("Error: clientul cautat nu exista!\n");
						}
					}

					/* comanda: broadcast <message>
					 * mai intai cer serverului o lista cu toti clientii activi
					 * apoi trimit tutor mesajul, exact ca la message*/
					if (strcmp (token, "broadcast") == 0) {
						char msg[BUFFLEN];
						char **clients;
						char message[BUFFLEN - 10];
						int no, k;
						int type = 1;

						memcpy (message, buffer + 10, BUFFLEN - 10);
						message[strlen(message)-1] = '\0';

						memset (msg, 0, BUFFLEN);
						memcpy (msg, &type, sizeof (int));
						send (server_sfd, msg, BUFFLEN, 0);

						recv (server_sfd, &no, sizeof(int), 0);
						clients = malloc (no * sizeof(char *));
						for (k = 0; k < no; k++) {
							clients[k] = malloc (sizeof(char) * NAME_LEN);
							recv (server_sfd, clients[k], NAME_LEN, 0);
						}

						for (k = 0; k < no; k++) {

							if (strcmp (clients[k], name) == 0) {
								continue;
							}
							/* aflu portul pentru fiecare client, si trimit mesajul */
							memset (msg, 0, BUFFLEN);
							type = 2;
							memcpy (msg, &type, sizeof (int));
							memcpy (msg + sizeof (int), clients[k], NAME_LEN);
							send (server_sfd, msg, BUFFLEN, 0);

							memset (msg, 0, BUFFLEN);
							recv (server_sfd, msg, BUFFLEN, 0);
							memcpy (&type, msg, sizeof (int));

							if (type == 1) {
								/* a fost gasit clientul cautat */
								ClientInfo cl_info;
								memcpy (&cl_info, msg + sizeof(int), sizeof(ClientInfo));
								sendMessage (cl_info.port, name, message, strlen(message));
							} else {
								printf ("Error: clientul cautat nu exista!\n");
							}
						}
					}

					/* comanda: sendfile <client_dest> <nume_fis> */
					if (strcmp (token, "sendfile") == 0) {
						char in_file[NAME_LEN], out_file[NAME_LEN];

						token = strtok (NULL, " \n");
						memset (to_client_name, 0, NAME_LEN);
						strcpy (to_client_name, token);
						token = strtok (NULL, " \n");
						strcpy (in_file, token);

						sprintf (out_file, "%s_fisier_primit", in_file);

						fld = open (in_file, O_RDONLY);

						/* scad 8 octeti, deoarece tb sa mai trimit si un header
						 * in care precizez tipul mesajului si dimensiunea datelor
						 * efective*/

						char msg[BUFFLEN];
						int type;
						FMessage fmsg;
						ClientInfo cl_info;
						cl_info = getInfoClient (to_client_name, server_sfd);

						/* daca exista clientul cautat */
						if (cl_info.status == 2) {

							type = 2;
							status = 1;//in curs de trimitere fisier

							memcpy (msg, &type, sizeof (int));
							strcpy (fmsg.from_name, name);
							strcpy (fmsg.file_name, out_file);
							memcpy (msg + sizeof (int), &fmsg, sizeof (FMessage));

							/* ma conectez la client */
							struct sockaddr_in cl_addr;

							cl_addr.sin_family = AF_INET;
							cl_addr.sin_port = cl_info.port;
							cl_addr.sin_addr = cl_info.ip_addr;

							to_client_sfd = socket(AF_INET, SOCK_STREAM, 0);

							if (to_client_sfd < 0)
								printf ("ERROR to_client\n");

							if (connect (to_client_sfd,(struct sockaddr*) &cl_addr, sizeof(cl_addr)) < 0) {
								printf ("Error in connect\n");
							}

							int n = send (to_client_sfd, msg, BUFFLEN, 0);
							printf ("Started sending file\n", n);
							/* pentru fisiere trimit 3 feluri de mesaje, care au
							 * tipul urmator:
							 * type = 2: recv trebuie sa deschida fisierul
							 * type = 3: recv primeste bacati din fisier
							 * type = 4: s-a terminat de trimis fisierul, rev trebuie
							 * sa inchida fisierul
							 */
						} else {
							printf ("Error: clientul cautat nu exista!\n");
							close (fld);
							status = 0;
						}

					}

					/* comanda: history */
					if (strcmp (token, "history") == 0) {
						int k;
						int tp;
						for (k = 0; k < h_counter; k++) {
							memcpy (&tp, history[k], sizeof (int));

							/* daca e mesaj */
							if (tp == 1) {
								HMessage m;
								memcpy (&m, history[k] + sizeof (int), sizeof (HMessage));
								printf ("%d: [%s][%s] %s\n", k, m.timestamp, m.name, m.msg);
							} else {
								/* altfel e de tipul: nume_client nume_fisier */
								FMessage fmsg;
								memcpy (&fmsg, history[k] + sizeof (int), sizeof (FMessage));
								printf ("%d: [%s] %s\n", k, fmsg.from_name, fmsg.file_name);
							}
						}
					}

					/* comanda: quit */
					if (strcmp (token, "quit") == 0) {
						printf ("Exiting\n");
						/* trimit mesaj serverului ca parasesc sistemul
						 * acesta este de tipul 4 */
						int tp = 4;
						char msg[BUFFLEN];
						memcpy (msg, &tp, sizeof (int));
						send (server_sfd, msg, BUFFLEN, 0);

						close (server_sfd);
						close (my_sfd);

						return 0;
					}



				} else {

					/* A venit ceva pe socket-ul inactiv, cel cu listen.
					 * Un client incearca sa se conecteze la mine
					 * il accept()
					 *
					 */
					if (i == my_sfd) {

						int newsockfd;
						struct sockaddr_in new_addr;
						int size = sizeof (struct sockaddr_in);
						newsockfd = accept (my_sfd, (struct sockaddr *) &new_addr, &size);
						FD_SET(newsockfd, &read_fds);

						if (newsockfd > fdmax) {
							fdmax = newsockfd;
						}

					} else {

						/* A venit ceva de la server */
						if (i == server_sfd) {
							memset (buffer, 0, BUFFLEN);
							count = recv (i, buffer, BUFFLEN, 0);
							int n;
							memcpy (&n, buffer, sizeof (int));
							if (count <= 0) {
								if (count == 0) {
									/* A iesit serverul. Trebuie sa ies si eu */
									close (server_sfd);
									close (my_sfd);
									printf ("Server is down. Exiting!\n");
									return -4; //Serverul a iesit
								}
							}
							int type;
							memcpy (&type, buffer, sizeof(int));
							/* type = 1 => am primit kick */
							if (type == 1) {
								close (server_sfd);
								close (my_sfd);
								printf ("Got kick from server!\n");
								return -3; //kicked
							}
						} else {
							/* a venit ceva de la alti clienti
							 * poate fi un mesaj (type = 1)
							 * sau un fisier (type = 2, 3, 4) */
							memset (buffer, 0, BUFFLEN);
							if ((count = recv(i, buffer, BUFFLEN, 0)) <= 0) {
								if (count == 0) {
									//printf ("SelectClient hang up\n");
									close(i);
									FD_CLR(i, &read_fds);
								}else {
									error ("Error in recv");
								}
							} else {
								memcpy (&type, buffer, sizeof (int));
								/* am primit un mesaj */
								if (type == 1) {
									char msg[BUFFLEN];
									char cl_name[NAME_LEN];
									depl = sizeof (int);
									memcpy (cl_name, buffer + depl, NAME_LEN);
									depl += NAME_LEN;
									memcpy (msg, buffer + depl, BUFFLEN - depl);

									time_t t;
									char timestamp[20];
									struct tm *time_now;
									time (&t);
		                            time_now = localtime(&t);
		                            strftime(timestamp, 20, "%T", time_now);

									printf ("[%s][%s] %s\n",
											timestamp, cl_name, msg);
									/* retin in history mesajul */
									HMessage hm;
									strcpy (hm.timestamp, timestamp);
									strcpy (hm.name, cl_name);
									strcpy (hm.msg, msg);
									int tp = 1; //tip mesaj
									memcpy (history[h_counter], &tp, sizeof (int));
									memcpy (history[h_counter] + sizeof (int),
											&hm, sizeof (HMessage));
									h_counter ++;

									if (h_counter == HLEN) {
										h_counter = 0;
									}

								}

								/* primesc un fisier
								 * type = 2: primesc numele fisierului */
								if (type == 2) {
									char filename[NAME_LEN];
									FMessage fmsg;
									memcpy (&fmsg, buffer + sizeof (int), sizeof (FMessage));
									strcpy (filename, fmsg.file_name);
									fld_to_recv = open (fmsg.file_name, O_WRONLY | O_TRUNC | O_CREAT,  0666);

									/* salvez in history numele mesajului */
									int tp = 2;
									memcpy (history[h_counter], &tp, sizeof (int));
									memcpy (history[h_counter] + sizeof (int),
											&fmsg, sizeof (FMessage));
									h_counter ++;

									if (h_counter == HLEN) {
										h_counter = 0;
									}
								}

								/* primesc "bucati" de fisier
								 * type = 3
								 */
								if (type == 3) {
									int depl = 2 * sizeof (int);
									int size;
									memcpy (&size, buffer + sizeof (int), sizeof (int));
									int n = write (fld_to_recv, buffer + depl, size);
								}

								/* am primit tot fisierul, trebuie sa-l inchid */
								if (type == 4) {
									close (fld_to_recv);
									printf ("File received\n");


								}
							}

						}
					}
				}


			}//end FD_ISSET
		}//end for
	}



	/*Inchidere socket*/
	close (server_sfd);
	close (my_sfd);


	return 0;
}
