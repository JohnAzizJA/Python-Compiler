class Car:

    def __init__(self, name):
        self.name = name

    def display(self):
        print(f"name: {self.name}")

def add(x, y):
    result = x + y
    print("The sum is:", result)
    return result

z = add(5, 3)

if z > 10:
    print("z is greater than 10")
    z -= 1
else:
    print("z is not greater than 10")
    z += 1
        
def main():
    car1 = Car("Honda")
    car1.display()
    print(car1.name.upper())

if __name__ == "__main__":
    main()
