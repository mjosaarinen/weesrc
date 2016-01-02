#/bin/bash
# Comparison for of gzip-like compressors

# original lengths and checksum

corpus=cantenbury
orlen=( $(stat -c %s $corpus/*) )
orfil=( $( ls $corpus ) )
nofil=${#orfil[*]}

TIMEFORMAT=$'%3Rs real / %3Us user / %3Ss sys'

# loop over tools 
for zz in gzip bzip2 xz ../wee
do
	# compress the corpus
	zd=`basename $zz`
	rm -rf $zd
	mkdir $zd
	cp $corpus/* $zd

	echo -n $zd$'\t    Compress: '
	time $zz $zd/*

	# new lengths and checks
	mylen=( $(stat -c %s $zd/*) )

	echo -n $zd$'\t  Decompress: '
	time $zz -d $zd/*
		
	for f in $(ls $corpus)
	do
		if ! cmp $corpus/$f $zd/$f >> /dev/null
		then echo $zd "!!! DECOMPRESSION ERROR WITH" $f "!!!"
		fi
	done
	rm -rf $zd
	
	(( av = 0 ))
	for ((i = 0; i < nofil; i++))
	do
		(( pp = 1000 * (orlen[i] - mylen[i]) / orlen[i] ))
		(( av += pp ))
		(( pc = pp / 10 ))
		(( pf = pp % 10 ))
		printf "%s\t%12d %3d.%d%%  %s\n" $zd ${mylen[i]} $pc $pf ${orfil[i]}
	done
	(( pp = av / nofil ))
	(( pc = pp / 10 ))
	(( pf = pp % 10 ))	 
	echo $zd$'\t============ ' $pc.$pf% " AVERAGE"

	echo
	
done

