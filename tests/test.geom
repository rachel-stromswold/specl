offset = 0.2
list = [1, 2, 3, 4]
prod_list = list[0]*list[1]*list[2]*list[3] + offset
acid_test = 100*offset - ((list[2] < list[1]) ? list[2] : list[1] + (list[1] > list[0]) ? list[1] : list[2])
acid_res = (acid_test == 16)
list[1] = 3
sum_list = list[0] + list[1] + list[2] + list[3]
assert(len(list) == 4)

//make sure that comments are actually ignored
//even if they take up multiple lines
gs = Gaussian_source("Ey", 1.5, 7.0, 3.0, 0.75, 6, 5.2, Box(vec(0,0,offset), vec(2*offset,.4,.2)))
