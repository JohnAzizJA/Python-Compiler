class Car:

    def _init_(self, name):
        self.name = name

    def printname(self):
        print(self.name)
car1 = Car("Honda")
car1.printname()

mySet = {4, 5, 6}
mytuble = (1, 2, 3)
mylist = [1, 2, 3]

x , y = 10, 20

x = 1 if True else 0
print("yes" if True else "no")
print("yes") if False else print("no")
y = 10 < (x if True else y)
my_list = [1, 2, 3 if False else 4]