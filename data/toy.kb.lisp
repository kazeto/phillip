(B (name gun_shot) (=> (^ (have x y :1.2) (gun y :1.2)) (shot x z :1.2)))
(B (name shot_kill) (=> (shot x y :1.3) (kill x y :1.2)))
(B (name kill_die) (=> (kill x y :1.2) (die y :1.0)))
(B (name sick_die) (=> (sick x :1.2) (die x :1.2)))
(B (name cough_sick) (=> (sick x :1.2) (cough x :1.2)))

(B (name sick_xor_kill) (xor (sick x) (kill x y)))
