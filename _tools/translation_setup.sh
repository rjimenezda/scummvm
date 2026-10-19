#!/bin/zsh
# setup.sh OUTNAME TemplateName
S=/Users/roman/git/scummvm/_games/_out/_templates; O=/Users/roman/git/scummvm/_games/_out/$1; mkdir -p $O/es; cp $S/$2.tsv $O/learn_translation.tsv; cp $S/$2.csv $O/lines.csv
tail -n +2 $O/learn_translation.tsv | awk -F'\t' '$2!=""{print $1"\t"$2}' > $O/es/all.tsv; n=$(wc -l < $O/es/all.tsv); per=$(( (n+7)/8 )); cd $O/es && split -l $per -d -a 1 all.tsv chunk && for f in chunk?; do mv $f $f.tsv; done; rm all.tsv; wc -l chunk?.tsv | tail -1
