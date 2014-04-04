(B (name gun_shot) (=> (^ (have x y) (gun y)) (shot x z)))
(B (name shot_kill) (=> (shot x y) (kill x y)))
(B (name kill_die) (=> (kill x y) (die y)))
(B (name sick_die) (=> (sick x) (die x)))
