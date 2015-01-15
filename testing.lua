function add(a, b)
	print(a + b);
end

function sub(a, b)
	return a - b
end

function pt(t)
	for key,value in pairs(t) do print(key,value) end
end

pt(a)
a = 6.3;

pt(__classes.Foo)

print(foo);
foo:bar("Nyan", 2);
foo.ind = 1;