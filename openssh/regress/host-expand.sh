#	$OpenBSD: host-expand.sh,v 1.3 2014/02/27 23:17:41 djm Exp $
#	Placed in the Public Domain.

tid="expand %h and %n"

echo 'PermitLocalCommand yes' >> $OBJ/ssh_proxy
printf 'LocalCommand printf "%%%%s\\n" "%%n" "%%h"\n' >> $OBJ/ssh_proxy

cat >$OBJ/expect <<EOE
somehost
127.0.0.1
EOE

for p in 2; do
	verbose "test $tid: proto $p"
	${SSH} -F $OBJ/ssh_proxy -$p somehost true >$OBJ/actual
	diff $OBJ/expect $OBJ/actual || fail "$tid proto $p"
done

