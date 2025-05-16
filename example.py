class Car:

    def __init__(self, name):
        self.name = name

    def display(self):
        print(f"name: {self.name}")

list = [1, 2, 3, 4, 5]
l = list[0]
tuple = (1, 2, 3, 4, 5)
t = tuple[1]
dict = {"key1": "value1", "key2": "value2"}
d = dict["key1"]

def add(x, y):
    result = x + y
    print("The sum is:", result)
    return result

z = add(5, 3)

if z > 10:
    print("z is greater than 10")
    z -= 1
elif z < 10:
    print("z is less than 10")
    z += 1
else:
    print("z is equal to 10")
        
def main():
    car1 = Car("Honda")
    car1.display()
    print(car1.name.upper())

    x = 1 if True else 2
    print("yes" if True else "no")
    y = 10 < (x if True else y)
    my_list = [1, 2, 3 if False else 4]

if __name__ == "__main__":
    main()
    # h, i = "h", "i"