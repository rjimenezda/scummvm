#!/bin/zsh
# finish.sh OUTNAME "Humongous Folder"
O=/Users/roman/git/scummvm/_games/_out/$1; cd $O || exit 1
for f in es/chunk?.es.tsv; do b=${f%.es.tsv}; [ $(wc -l <$f) = $(wc -l <$b.tsv) ] || echo "count mismatch $f"; diff <(cut -f1 $f) <(cut -f1 $b.tsv) >/dev/null || echo "offset mismatch $f"; awk -F'\t' 'NF!=2{print FILENAME": "$0}' $f; done
sed -i '' -E 's/\(PU[0-9]\) ?//g; s/\[[A-Z0-9_]+\.[0-9A-Z]+\]//g' es/chunk?.es.tsv
python3 -I /Users/roman/git/scummvm/_tools/merge_translation.py learn_translation.tsv learn_translation.es.tsv es/chunk?.es.tsv || { echo MERGE FAILED; exit 1; }
cp learn_translation.es.tsv "/Users/roman/scummvm/humongous/$2/learn_translation.tsv" && echo "installed in $2"
awk 'NR%300==0' learn_translation.es.tsv
