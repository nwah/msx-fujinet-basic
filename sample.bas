10 url$="N:http://noahburney.net/"
20 _nopen(url$,4,0)
30 _nstatus(url$,bw%,co%,er%)
40 _nread(url$,bw%,res$,s%)
50 print res$
run


10 print "HOSTS"
20 _floadhostslots
30 for h=0 to 7
40 _fgethostslot(h, s$)
50 print h;":";s$
60 next h
65 print "DEVICES"
70 _floaddevslots
80 for d=0 to 7
100 _fgetdevslothost(d, hs%)
105 _fgetdevslotfile(d, f$)
110 print d;":";hs%;"/";f$
120 next d
run
