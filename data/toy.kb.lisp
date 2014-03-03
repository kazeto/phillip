(B (name syntax.of#a) (=> (of-in x y) (^ (own-vb e) (agt e y) (obj e x)) :syntax))
(B (name syntax.of#b) (=> (of-in x y) (cmp y x) :syntax))
(B (name syntax.with) (=> (with-in x y) (^ (own-vb e) (agt e x) (obj e y)) :syntax))
(B (name syntax.may#a) (=> (may-md m) (potential-adj m) :syntax))
(B (name syntax.may#b) (=> (^ (may-md m) (mod m e) (agt e x)) (^ (potential-adj e) (mod e x)) :syntax))
(B (name syntax.may#c) (=> (^ (may-md m) (mod m e) (obj e x)) (^ (potential-adj e) (mod e x)) :syntax))
(B (name syntax.cmp#a) (=> (cmp x y) (^ (agt e x) (obj e y)) :syntax))
(B (name syntax.cmp#b) (=> (cmp x y) (^ (agt e y) (obj e x)) :syntax))

(B (name inherit.mod) (=> (^ (inherit x y) (mod a y)) (mod a x) :syntax))

(B (name proper_noun.nato) (=> (nato-nn x) (^ (atlantic-nn y1) (organization-nn y2) (cmp y1 y2)) :propernoun))

(B (name closed_deal_with#a) (=> (^ (close-vb e1) (agt e1 x1) (deal-nn x2) (obj e1 x2) (with-in x2 x3))
                                 (^ (buy-vb e2) (agt e2 x1) (obj e2 x3)) :multiword))
(B (name closed_deal_with#b) (=> (^ (close-vb e1) (agt e1 x1) (deal-nn x2) (obj e1 x2) (with-in x2 x3))
                                 (^ (buy-vb e2) (agt e2 x3) (obj e2 x1)) :multiword))
(B (name closed_deal_with#c) (=> (^ (close-vb e1) (agt e1 x1) (deal-nn x2) (obj e1 x2) (with-in x2 x3))
                                 (^ (buy-vb e2) (agt e2 x1) (obj e2 u1) (from-in e2 x3) (!= x3 u1)) :multiword))
(B (name closed_deal_with#d) (=> (^ (close-vb e1) (agt e1 x1) (deal-nn x2) (obj e1 x2) (with-in x2 x3))
                                 (^ (buy-vb e2) (agt e2 x3) (obj e2 u1) (from-in e2 x1) (!= x1 u1)) :multiword))

(B (name role_preference) (=> (role-nn x) (^ (play-vb e) (obj e x)) :preference))
(B (name cause) (=> (^ (dissent-vb e1) (agt e1 x)) (^ (intractable-adj e2) (mod e2 x)) :cause))
