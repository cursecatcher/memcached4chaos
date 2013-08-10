# !/bin/bash
echo "Deleting old Makefiles."
rm Balancer/Makefile Server/Makefile Setter/Makefile Getter/Makefile

echo "Creating new Makefiles."
cbp2make -in Balancer/Balancer.cbp -out Balancer/Makefile
cbp2make -in Server/Server.cbp -out Server/Makefile
cbp2make -in Setter/Setter.cbp -out Setter/Makefile
cbp2make -in Getter/Getter.cbp -out Getter/Makefile

