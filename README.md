# liscript
liscript는 lisp의 문법을 따온 prototype-based 언어입니다.

# example code
다음은 버블 정렬 코드입니다.
```
>> (setl arr (array 1 3 2 8 4 6 5))
[
  1,
  3,
  2,
  8,
  4,
  6,
  5
]
>> (setl sort (func (ar) (do
--   (setl i 0)
--   (while (< i (ar size))
--     (do
--       (setl j (+ i 1))
--       (while (< j (ar size))
--         (do
--           (if (> (ar get (- j 1)) (ar get j))
--             (do
--               (setl tmp (ar get (- j 1)))
--               (ar set (- j 1) (ar get j))
--               (ar set j tmp))
--             ())
--           (setl j (+ j 1))))
--       (setl i (+ i 1))))
--     ar)))
(func (ar) (..))
>> (() sort arr)
[
  1,
  2,
  3,
  4,
  5,
  6,
  8
]
```

# reference

#### A. language reference

liscript는 lisp의 문법을 따온 prototype-based 언어입니다.

expression의 평가는 다음 규칙을 따릅니다.
 1) number와 string이라면 그대로 평가
 2) atom keyword라면 정의된 동작대로
 3) keyword가 아닌 atom이라면 (getl [atom])
 4) list라면
  4a) empty list라면 undefined
  4b) 첫째 항목이 list keyword라면 정의된 동작대로
  4d) 첫째 항목을 평가해서 object이고 둘째 항목이 atom이면 가능할 경우 멤버 함수 호출
  4c) 둘째 항목을 평가해서 function이라면 첫째 항목을 this로, 셋째 항목 이후를 argument로 하여 함수 호출
  4e) 전부 아니라면 ListEvaluateError 예외를 던짐

this값은 초기에는 global입니다. 함수를 호출할 때 this값을 지정해야 합니다.

ex)
```
    (setf this asdf (new Object)) // global에 새 object를 asdf라는 이름으로 넣음
    (asdf (func ()
        (setf this qwer (new Object)))) // asdf에 새 object를 qwer라는 이름으로 넣음
```

atom keyword: global this undefined null true false prev arguments ...

list keyword: func new array getf setf getl setl geti seti deli do if while + -/ % & idiv imod | ^ and or not = /= < <= > >=

conditional로 쓰이는 값이 true이거나 null이 아닌 object라면 참으로 취급됩니다.

conditional로 쓰이는 값이 false, null, undefined라면 거짓으로 취급됩니다.

conditional로 쓰이는 값이 위 두 경우가 아니라면 invalid_conditional 예외가 발생합니다.

[ ]로 표시된 것은 생략할 수 없는 항목입니다.

/ /로 표시된 것은 생략 가능한 항목입니다.

###### atom keywords

global
 * 전역 객체입니다.

this
 * this 값입니다.

undefined
 * undefined 값입니다.

null
 * null 값입니다.

prev
 * do 또는 while에서 이전에 평가된 값입니다. 없다면 undefined입니다.

arguments
 * 호출부가 넘긴 함수 실인수입니다. 호출된 함수 내부가 아닌 경우 undefined입니다.

...
 * func 키워드에서 가변 인수를 나타냅니다.

###### list keywords

func: (func /name/ (/par1/ ... /.../) [expr])
 * 함수를 만듭니다. name이 있을 경우 만든 함수를 지정한 atom에 setl하고, 생성자가 되기 위한 prototype을 생성합니다.

new: (new [function] /arg1/ ...)
 * 새 object를 만들고 생성자를 호출해 초기화합니다.

array: (array /item1/ ...)
 * 새 array를 만들고 지정된 항목으로 초기화합니다.

getf: (getf /object/ [atom])
 * object에서 값을 가져옵니다. object 항목이 생략됬다면 this에서 가져옵니다.

setf: (setf /object/ [atom] [expr])
 * object에 값을 넣습니다. object 항목이 생략됬다면 this에 넣습니다.

getl: (getl [atom])
 * 지역 변수에 값을 넣습니다.

setl: (setl [atom] [expr])
 * 지역 변수에 값을 넣습니다.

geti: (geti [object] [string])
 * object에서 지정된 이름의 field를 가져옵니다.

seti: (seti [object] [string] [expr])
 * object의 지정된 이름의 field에 값을 넣습니다.

deli: (deli [object] [string])
 * object에서 지정된 이름의 field를 삭제합니다.

do: (do [expr1] /expr2/ ...)
 * expr들을 순차적으로 평가합니다. do 구문의 값은 맨 마지막 expr의 값이 됩니다.

if: (if [condition] [expr1] [expr2])
 * condition이 참이라면 expr1을 평가한 값이 됩니다.
 * condition이 거짓이라면 expr2를 평가한 값이 됩니다.

while: (while [condition] [expr])
 * condition이 참일 동인 expr를 반복 실행합니다. while 구문의 값은 마지막으로 평가된 값이 됩니다.

+: (+ [expr1] /expr2/ ...)
 * expr들을 순차적으로 평가한 후 그 합을 계산합니다.

-: (- [expr])
 * expr를 평가하고 부호를 반전시킵니다.

-: (- [expr1] [expr2])
 * expr1의 평가값에서 expr2의 평가값을 뺍니다.

\*: (\* [expr1] /expr2/ ...)
 * expr들을 순차적으로 평가한 후 그 곱을 계산합니다.

/: (/ [expr1] [expr2])
 * expr1의 평가값에서 expr2의 평가값을 나누어 그 몫을 계산합니다.

%: (/ [expr1] [expr2])
 * expr1의 평가값에서 expr2의 평가값을 나누어 그 나머지을 계산합니다.

&: (& [expr1] [expr2])
 * expr1의 평가값과 expr2의 평가값의 bitwise AND를 계산합니다.

|: (& [expr1] [expr2])
 * expr1의 평가값과 expr2의 평가값의 bitwise OR을 계산합니다.

^: (^ [expr1] [expr2])
 * expr1의 평가값과 expr2의 평가값의 bitwise XOR을 계산합니다.

and: (and [expr1] /expr2/ ...)
 * expr들의 평가값의 logical AND를 계산합니다. short circuit가 적용됩니다.

or: (and [expr1] /expr2/ ...)
 * expr들의 평가값의 logical OR을 계산합니다. short circuit가 적용됩니다.

not: (and [expr])
 * expr의 평가값의 logical NOT을 계산합니다.

=: (= [expr1] [expr2])
 * expr1의 평가값과 expr2의 평가값이 같은지 확인합니다.

/=: (/= [expr1] [expr2])
 * expr1의 평가값과 expr2의 평가값이 다른지 확인합니다.

<: (< [expr1] [expr2])
 * expr1의 평가값이 expr2의 평가값보다 작은지 확인합니다.

<=: (<= [expr1] [expr2])
 * expr1의 평가값이 expr2의 평가값보다 작거나 같은지 확인합니다.

\>: (> [expr1] [expr2])
 * expr1의 평가값이 expr2의 평가값보다 큰지 확인합니다.

\>=: (>= [expr1] [expr2])
 * expr1의 평가값이 expr2의 평가값보다 크거나 같은지 확인합니다.

#### B. built-in library reference

class **Array**
  * 배열을 나타내는 클래스입니다.
  * 생성자로 직접 생성하는 대신 array 키워드를 사용해 생성해야 합니다.
  * **ctor**: func Array()
    * 생성자입니다. 생성자로 직접 생성하는 대신 array 키워드를 사용해 생성해야 합니다.
  * func **size**()
    * 배열의 크기를 가져옵니다.
  * func **get**(index: number)
    * index번째 항목을 가져옵니다.
  * func **set**(index: number, val)
    * index번째 항목에 값을 넣습니다.

object **replConfig**
  * repl에 관련된 설정입니다.
  * field **dumpExpr**: boolean
    * expr 평가 전 구문 분석 결과를 출력할지 여부입니다. 기본값은 false입니다.

object **console**
  * 콘솔 입출력을 담당합니다.
  * func **dump**(...)
    * obj의 정보를 화면에 출력합니다.
  * func **readLine**() -> string
    * 한 줄을 표준 입력에서 읽어들입니다.

func **parseFloat**(str: string) -> number
  * 문자열을 부동 소수점 숫자로 바꿉니다.
