Tanto el informe como la presentacion se encuentran en la carpeta del respositorio (pc-2018-07) bajo en nombre de 
informe.pdf y presentacion.pdf.
Para poder generar una version ejecutable, correr make dentro de la carpeta del respositorio (pc-2018-07). Estos 
ejecutables estaran ubicados en la misma carpeta
Para correr el server:
./server -p puerto_tcp -l ip_tcp -P puerto_sctp -L ip_sctp
Puede no ponerse argumentos.
Para correr client:
./client ip_server:puerto_server_sctp -mi -cj mediatype
con 1 <= i <= 4 y  0 <= j <= 4
La contrasena para ingresar es tpprotos