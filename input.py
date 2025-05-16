# Sample Python code for testing
def hello(name):
    print("Hello, " + name + "!")
    return True

class Person:
    def __init__(self, name):
        self.name = name
    
    def greet(self):
        return "Hello, my name is " + self.name

# Main code
if __name__ == "__main__":
    person = Person("John")
    hello(person.name)
    print(person.greet())