/*
 ============================================================================
 Name        : Memory.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "Memory.h"
#include "sockets.h"

void *inputFunc(void *);
pthread_mutex_t lock;

int cant_paginas;
void* memoriaPrincipal;
t_list* tabla_segmentos;
Tabla_paginas tabla_paginas;

int main() {

	struct addrinfo hints;
	struct addrinfo *serverInfo;
	char* ip;
	char* puerto;

	cant_paginas = floor(MEMORY_SIZE / sizeof(Pagina));
	memoriaPrincipal = malloc(MEMORY_SIZE);
	tabla_segmentos = list_create();
	tabla_paginas.renglones = malloc(cant_paginas * sizeof(Renglon_pagina));

	//hardcode tablas
	Pagina franco;
	franco.timeStamp = 1557972674;
	franco.key = 20;
	strcpy(franco.value, "Franco");

	Pagina santi;
	santi.timeStamp = 1557972674;
	santi.key = 21;
	strcpy(santi.value, "Santi");

	Renglon_pagina renglonFranco;
	renglonFranco.numero = 0;
	renglonFranco.modificado = 0;
	renglonFranco.offset = renglonFranco.numero*sizeof(Pagina);

	Renglon_pagina renglonSanti;
	renglonSanti.numero = 1;
	renglonSanti.modificado = 0;
	renglonSanti.offset = renglonSanti.numero*sizeof(Pagina);

	memcpy((memoriaPrincipal + renglonFranco.offset), &franco, sizeof(Pagina));
	memcpy((memoriaPrincipal + renglonSanti.offset), &santi, sizeof(Pagina));

	tabla_paginas.renglones[0] = renglonFranco;
	tabla_paginas.renglones[1] = renglonSanti;

	Segmento tabla1;
	strcpy(tabla1.path, "TABLA1");
	tabla1.numeros_pagina = list_create();
	list_add(tabla1.numeros_pagina, 0);
	list_add(tabla1.numeros_pagina, 1);

	list_add(tabla_segmentos, &tabla1);

	//Harcode tablas

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	abrir_log();

	t_config *conection_conf;
	abrir_con(&conection_conf);

	ip = config_get_string_value(conection_conf, "IP");
	puerto = config_get_string_value(conection_conf, "PUERTO_DEST");

	getaddrinfo(ip, puerto, &hints, &serverInfo);// Carga en serverInfo los datos de la conexion

	config_destroy(conection_conf);

	/*
	 * 	Ya se quien y a donde me tengo que conectar... ������Y ahora?
	 *	Tengo que encontrar una forma por la que conectarme al server... Ya se! Un socket!
	 *
	 * 	Obtiene un socket (un file descriptor -todo en linux es un archivo-), utilizando la estructura serverInfo que generamos antes.
	 *
	 */
	int lfsSocket;
	lfsSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);

	/*
	 * 	Perfecto, ya tengo el medio para conectarme (el archivo), y ya se lo pedi al sistema.
	 * 	Ahora me conecto!
	 *
	 */
	connect(lfsSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);	// No lo necesitamos mas

	printf("Conectado al LFS \n");

	enviar_handshake(MEMORY, lfsSocket);
	/*

	 *
	 *  Estas y otras preguntas existenciales son resueltas getaddrinfo();
	 *
	 *  Obtiene los datos de la direccion de red y lo guarda en serverInfo.
	 *
	 */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(NULL, PUERTO, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE

	/*
	 * 	Descubiertos los misterios de la vida (por lo menos, para la conexion de red actual), necesito enterarme de alguna forma
	 * 	cuales son las conexiones que quieren establecer conmigo.
	 *
	 * 	Para ello, y basandome en el postulado de que en Linux TODO es un archivo, voy a utilizar... Si, un archivo!
	 *
	 * 	Mediante socket(), obtengo el File Descriptor que me proporciona el sistema (un integer identificador).
	 *
	 */
	/* Necesitamos un socket que escuche las conecciones entrantes */
	int listenningSocket;
	listenningSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);

	/*
	 * 	Perfecto, ya tengo un archivo que puedo utilizar para analizar las conexiones entrantes. Pero... ������Por donde?
	 *
	 * 	Necesito decirle al sistema que voy a utilizar el archivo que me proporciono para escuchar las conexiones por un puerto especifico.
	 *
	 * 				OJO! Todavia no estoy escuchando las conexiones entrantes!
	 *
	 */

	int waiting = 1;
	if (bind(listenningSocket, serverInfo->ai_addr, serverInfo->ai_addrlen)
			!= -1) {
		waiting = 0;
	}
	while (waiting) {
		if (bind(listenningSocket, serverInfo->ai_addr, serverInfo->ai_addrlen)
				!= -1) {
			waiting = 0;
		}
		sleep(5);
	}

	freeaddrinfo(serverInfo); // Ya no lo vamos a necesitar

	/*
	 * 	Ya tengo un medio de comunicacion (el socket) y le dije por que "telefono" tiene que esperar las llamadas.
	 *
	 * 	Solo me queda decirle que vaya y escuche!
	 *
	 */
	listen(listenningSocket, BACKLOG); // IMPORTANTE: listen() es una syscall BLOQUEANTE.

	printf("Esperando kernel... \n");

	/*
	 * 	El sistema esperara hasta que reciba una conexion entrante...
	 * 	...
	 * 	...
	 * 	BING!!! Nos estan llamando! ������Y ahora?
	 *
	 *	Aceptamos la conexion entrante, y creamos un nuevo socket mediante el cual nos podamos comunicar (que no es mas que un archivo).
	 *
	 *	������Por que crear un nuevo socket? Porque el anterior lo necesitamos para escuchar las conexiones entrantes. De la misma forma que
	 *	uno no puede estar hablando por telefono a la vez que esta esperando que lo llamen, un socket no se puede encargar de escuchar
	 *	las conexiones entrantes y ademas comunicarse con un cliente.
	 *
	 *			Nota: Para que el listenningSocket vuelva a esperar conexiones, necesitariamos volver a decirle que escuche, con listen();
	 *				En este ejemplo nos dedicamos unicamente a trabajar con el cliente y no escuchamos mas conexiones.
	 *
	 */
	struct sockaddr_in addr; // Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(addr);

	int socketCliente = accept(listenningSocket, (struct sockaddr *) &addr,
			&addrlen);

	if (!recibir_handshake(KERNEL, socketCliente)) {
		printf("Handshake invalido \n");
		return 0;
	}

	/*
	 * 	Ya estamos listos para recibir paquetes de nuestro cliente...
	 *		 *
	 *	Cuando el cliente cierra la conexion, recieve_and_deserialize() devolvera 0.
	 */

	int status = 1;		// Estructura que manjea el status de los recieve.

	printf("Cliente conectado. Esperando Envío de mensajessss.\n");

	t_describe describe;
	t_metadata meta;
	meta.consistencia = SC;
	strcpy(meta.nombre_tabla, "TABLA_STRONG");
	t_metadata meta2;
	meta2.consistencia = EC;
	strcpy(meta2.nombre_tabla, "TABLA_EVENTUAL");
	describe.cant_tablas = 2;
	describe.tablas = malloc(2 * sizeof(t_metadata));
	describe.tablas[0] = meta;
	describe.tablas[1] = meta2;

	printf("Tabla %s \n", describe.tablas[0].nombre_tabla);
	printf("Tabla %s \n", describe.tablas[1].nombre_tabla);

	char* serializedPackage;
	serializedPackage = serializarDescribe(&describe);
	send(socketCliente, serializedPackage,
			2 * sizeof(t_metadata) + sizeof(describe.cant_tablas), 0);
	dispose_package(&serializedPackage);
	free(describe.tablas);

	/*
	 //thread
	 pthread_t threadL;
	 int iret1;

	 if (pthread_mutex_init(&lock, NULL) != 0) {
	 printf("\n mutex init failed\n");
	 return 1;
	 }

	 iret1 = pthread_create(&threadL, NULL, inputFunc, (void*) lfsSocket);
	 if (iret1) {
	 fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
	 exit(EXIT_FAILURE);
	 }

	 */
	//thread
	int headerRecibido;

	while (status) {

		headerRecibido = recieve_header(socketCliente);

		status = headerRecibido;

		if (status) {
			if (headerRecibido == SELECT) {
				t_PackageSelect package;
				status = recieve_and_deserialize_select(&package,
						socketCliente);

				ejectuarComando(headerRecibido, &package);

				package.header = SELECT;
				//send_package(headerRecibido, &package, lfsSocket);

				free(package.tabla);
			} else if (headerRecibido == INSERT) {
				t_PackageInsert package;
				status = recieve_and_deserialize_insert(&package,
						socketCliente);

				ejectuarComando(headerRecibido, &package);
				package.header = INSERT;
				//send_package(headerRecibido, &package, lfsSocket);
			}

		}

		//pthread_mutex_lock(&lock);
		//status = recieve_and_deserialize(&package, socketCliente);
		//pthread_mutex_unlock(&lock);

		//fill_package(&packageEnvio, &username);

		// Ver el "Deserializando estructuras dinamicas" en el comentario de la funcion.

	}

	printf("Cliente Desconectado.\n");
	/*
	 * 	Terminado el intercambio de paquetes, cerramos todas las conexiones y nos vamos a mirar Game of Thrones, que seguro nos vamos a divertir mas...
	 *
	 *
	 * 																					~ Divertido es Disney ~
	 *
	 */
	//pthread_exit(&threadL);
	//pthread_mutex_destroy(&lock);
	close(socketCliente);
	close(listenningSocket);
	list_destroy(tabla_segmentos);
	free(memoriaPrincipal);
	free(tabla_paginas.renglones);
	log_destroy(g_logger);

	/* See ya! */

	return 0;
}

Segmento* buscarSegmento(char* tablaABuscar){

	int esDeLaTabla(Segmento *segmento){
		if(strcmp(segmento->path,tablaABuscar) == 0){
			return 1;
		}
		return 0;
	};

	return (Segmento*)list_find(tabla_segmentos,(int)&esDeLaTabla);
}

Pagina* buscarPagina(int keyBuscado,Segmento* segmento){

	Pagina* paginaEncontrada = NULL;

	void esDeLaTabla(int key){
		Renglon_pagina* renglon = tabla_paginas.renglones + key;
		int offsetPagina = renglon->offset;
		int keyPagina = ((Pagina*)(memoriaPrincipal + offsetPagina))->key;

		if (keyBuscado == keyPagina) {
			paginaEncontrada = ((Pagina*)(memoriaPrincipal + offsetPagina));
		}

		};

	list_iterate(segmento->numeros_pagina,&esDeLaTabla);

	return paginaEncontrada;
}

void ejectuarComando(int header, void* package) {
	switch (header) {
	case SELECT:
		ejecutarSelect((t_PackageSelect*) package);
		break;
	case INSERT:
		ejecutarInsert((t_PackageInsert*) package);
		break;
	}
}

void ejecutarSelect(t_PackageSelect* select) {
	Segmento* segmento_encontrado = buscarSegmento(select->tabla);
		if (segmento_encontrado != NULL) {

			Pagina* pagina_encontrada =	buscarPagina(select->key,segmento_encontrado);

			if (pagina_encontrada != NULL) {
				printf("El value es: %s \n", pagina_encontrada->value);

			} else {
				printf("No tengo ese registro \n");
			}
		} else {
			printf("No tengo esa tabla \n");
		}
}

void ejecutarInsert(t_PackageInsert* package) {
	printf("INSERT recibido (Tabla: %s, Key: %d, Value: %s, Timestamp: %d)\n",
			package->tabla, package->key, package->value, package->timestamp);
}

void abrir_con(t_config** g_config) {

	(*g_config) = config_create(CONFIG_PATH);

}

void abrir_log(void) {

	g_logger = log_create("memory_global.log", "memory", 0, LOG_LEVEL_INFO);

}

void send_package(int header, void* package, int lfsSocket) {

	char* serializedPackage;
	switch (header) {
	case SELECT:
		serializedPackage = serializarSelect((t_PackageSelect*) package);
		send(lfsSocket, serializedPackage,
				((t_PackageSelect*) package)->total_size, 0);

		break;
	case INSERT:
		serializedPackage = serializarInsert((t_PackageInsert*) package);
		send(lfsSocket, serializedPackage,
				((t_PackageInsert*) package)->total_size, 0);

		break;
	}
	dispose_package(&serializedPackage);

}

/*
 void *inputFunc(void* serverSocket)

 {
 int enviar = 1;
 t_PackagePosta package;
 package.message = malloc(MAX_MESSAGE_SIZE);
 char *serializedPackage;

 //t_PackageRec packageRec;
 //int status = 1;		// Estructura que manjea el status de los recieve.

 printf(
 "Bienvenido al sistema, puede comenzar a escribir. Escriba 'exit' para salir.\n");

 int ingresoCorrecto;
 while (enviar) {

 ingresoCorrecto = 1;
 fill_package(&package); // Completamos el package, que contendra los datos del mensaje que vamos a enviar.

 if (package.header == ERROR) {
 ingresoCorrecto = 0;
 printf("Comando no reconocido\n");
 } 		// Chequeamos si el usuario quiere salir.

 if (package.header == -1) {
 enviar = 0;
 } 		// Chequeamos si el usuario quiere salir.

 if (enviar && ingresoCorrecto) {
 serializedPackage = serializarOperandos(&package);// Ver: ������Por que serializacion dinamica? En el comentario de la definicion de la funcion.
 send((int) serverSocket, serializedPackage, package.total_size, 0);
 dispose_package(&serializedPackage);

 //status = recieve_and_deserialize(&packageRec, serverSocket);
 //if (status) printf("%s says: %s", packageRec.username, packageRec.message);
 }

 }

 printf("Desconectado.\n");


 free(package.message);

 }

 */

/*
 typedef struct t_Package {
 uint32_t message_long;
 char* message;
 uint32_t total_size;
 } t_Package;
 */
