TODO
====

# Functional-predicate の性質でありえない組み合わせを検知する処理

- Symmetric と Asymmetric は同時には成り立たない
- Symmetric かつ Right-unique は同時には成り立たない


# Functional-predicate の性質に対応した処理の実装

## Irreflexive

- r(x,x) は矛盾である

- むしろ Irreflexive でない関係の方が少ない気がするので, デフォルトにしてもいいかも

## Symmetric

- r(x,y) が成り立つ場合は r(y,x) も成り立つ

- リテラル生成時に変数の並びを正規化する
- 単一化した時に2つの可能性を考える
    - r(x,y) ^ r(a,b) => (x=a) ^ (y=b)
    - r(x,y) ^ r(a,b) => (x=b) ^ (y=a)

## Asymmetric

- r(x,y) が成り立つ場合は !r(y,x) も成り立つ

## Transitive

- r(x,y) かつ r(y,z) なら r(x,z) が成り立つ

## Right-unique

- r(x,y) かつ r(x,z) なら (y=z) が成り立つ


# 等価仮説に関する処理を Functional-predicate の処理と同一化する

- 等価関係も Symmetric かつ Transitive な関係である
- ほんとに出来るの？
    - 他の Functional-predicate は単一化操作の対象になるが, 等価仮説はならない


# エラーメッセージのフォーマットを統一する

- 不適切なフォーマットで書かれた入力が与えられた時のメッセージを統一したい
