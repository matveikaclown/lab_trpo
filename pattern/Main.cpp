#include <iostream>
#include <cassert>
#include <string> 

struct Transformer;
struct Number;
struct BinaryOperation;
struct FunctionCall;
struct Variable;

struct Expression { //базовая абстрактная структура
	virtual ~Expression() { } //виртуальный деструктор

	virtual double evaluate() const = 0; //абстрактный метод «вычислить»
	virtual Expression* transform(Transformer* tr) const = 0;
	virtual std::string print() const = 0;//абстрактный метод печать
};


struct Transformer { //pattern Visitor
	virtual ~Transformer() {}

	virtual Expression* transformNumber(Number const*) = 0;
	virtual Expression* transformBinaryOperation(BinaryOperation const*) = 0;
	virtual Expression* transformFunctionCall(FunctionCall const*) = 0;
	virtual Expression* transformVariable(Variable const*) = 0;
};


struct Number : Expression {// стуктура «Число»
public:
	Number(double value) : value_(value) {} //конструктор
	~Number() {}//деструктор, тоже виртуальный

	double value() const { return value_; } // метод чтения значения числа
	double evaluate() const { return value_; } // реализация виртуального метода «вычислить»
	std::string print() const { return std::to_string(this->value_); }
	Expression* transform(Transformer* tr) const { return tr->transformNumber(this); }

private:
	double value_; // само вещественное число
};


struct BinaryOperation : Expression { // «Бинарная операция»
public:
	BinaryOperation(Expression const* left, int op, Expression const* right) : left_(left), op_(op), right_(right) { assert(left_ && right_); }
	~BinaryOperation() {
		delete left_;
		delete right_;
	}

	enum {
		PLUS = '+',
		MINUS = '-',
		DIV = '/',
		MUL = '*'
	};
	Expression const* left() const { return left_; } // чтение левого операнда
	Expression const* right() const { return right_; } // чтение правого операнда
	int operation() const { return op_; } // чтение символа операции
	double evaluate() const { // реализация виртуального метода «вычислить»
		double left = left_->evaluate(); // вычисляем левую часть
		double right = right_->evaluate(); // вычисляем правую часть
		switch (op_) { // в зависимости от вида операции, складываем, вычитаем, умножаем или делим левую и правую части
		case PLUS: return left + right;
		case MINUS: return left - right;
		case DIV: return left / right;
		case MUL: return left * right;
		}
	}
	Expression* transform(Transformer* tr) const { return tr->transformBinaryOperation(this); }
	std::string print() const { return this->left_->print() + std::string(1, this->op_) + this->right_->print(); }

private:
	Expression const* left_;
	Expression const* right_;
	int op_;
};


struct FunctionCall : Expression {
public:
	FunctionCall(std::string const& name, Expression const* arg) : name_(name), arg_(arg) {
		assert(arg_);
		assert(name_ == "sqrt" || name_ == "abs");
	} // разрешены только вызов sqrt и abs
	~FunctionCall() { delete arg_; } // освобождаем память в деструкторе

	std::string const& name() const { return name_; }
	Expression const* arg() const { return arg_; }// чтение аргумента функции
	virtual double evaluate() const { // реализация виртуального метода «вычислить»
		if (name_ == "sqrt")
			return sqrt(arg_->evaluate()); // либо вычисляем корень квадратный
		return fabs(arg_->evaluate());
	} // либо модуль — остальные функции запрещены
	std::string print() const { return this->name_ + "(" + this->arg_->print() + ")"; }
	Expression* transform(Transformer* tr) const { return tr->transformFunctionCall(this); }

private:
	std::string const name_;
	Expression const* arg_;
};


struct Variable : Expression { 
public:
	Variable(std::string const& name) : name_(name) {}

	std::string const& name() const { return name_; } // чтение имени переменной
	double evaluate() const { return 0.0; } // реализация виртуального метода «вычислить»
	std::string print() const { return this->name_; }
	Expression* transform(Transformer* tr) const { return tr->transformVariable(this); }

private:
	std::string const name_; // имя переменной
};


struct CopySyntaxTree : Transformer {
public:
	Expression* transformNumber(Number const* number) {
		Expression* exp = new Number(number->value());
		return exp;
	}
	Expression* transformBinaryOperation(BinaryOperation const* binop) {
		Expression* exp = new BinaryOperation((binop->left())->transform(this), binop->operation(), (binop->right())->transform(this));
		return exp;
	}
	Expression* transformFunctionCall(FunctionCall const* fcall) {
		Expression* exp = new FunctionCall(fcall->name(), (fcall->arg())->transform(this));
		return exp;
	}
	Expression* transformVariable(Variable const* var) {
		Expression* exp = new Variable(var->name());
		return exp;
	}
};


struct FoldConstants : Transformer {
public:
	Expression* transformNumber(Number const* number) {
		Expression* exp = new Number(number->value());
		return exp; // числа не сворачиваются, поэтому просто возвращаем копию
	}
	Expression* transformBinaryOperation(BinaryOperation const* binop) {
		Expression* nleft = (binop->left())->transform(this); // рекурсивно уходим в левый операнд, чтобы свернуть
		Expression* nright = (binop->right())->transform(this); // рекурсивно уходим в правый операнд, чтобы свернуть
		int noperation = binop->operation();
		BinaryOperation* nbinop = new BinaryOperation(nleft, noperation, nright); // Создаем новый объект типа BinaryOperation с новыми указателями
		Number* nleft_is_number = dynamic_cast<Number*>(nleft); //Проверяем на приводимость указателей к типу Number
		Number* nright_is_number = dynamic_cast<Number*>(nright);
		if (nleft_is_number && nright_is_number) {
			Expression* result = new Number(binop->evaluate()); // Вычисляем значение выражения
			delete nbinop; 
			return result;
		}
		return nbinop;
	}
	Expression* transformFunctionCall(FunctionCall const* fcall) {
		Expression* arg = (fcall->arg())->transform(this); // Создаем указатель на аргумент
		std::string const& nname = fcall->name(); // рекурсивно сворачиваем аргумент
		FunctionCall* nfcall = new FunctionCall(nname, arg); // Создаем новый объект типа FunctionCall с новым указателем
		Number* arg_is_number = dynamic_cast<Number*>(arg); // Проверяем на приводимость указателя к типу Number
		if (arg_is_number) { // если аргумент — число
			Expression* result = new Number(fcall->evaluate());// Вычисляем значение выражения
			delete nfcall;
			return result;
		}
		return nfcall;
	}
	Expression* transformVariable(Variable const* var) {
		Expression* exp = new Variable(var->name());
		return exp; // переменные не сворачиваем, поэтому просто возвращаем копию
	}
};


int main() {
	/*
		//------------------------------------------------------------------------------
		Expression* e1 = new Number(1.234);
		Expression* e2 = new Number(-1.234);
		Expression* e3 = new BinaryOperation(e1, BinaryOperation::DIV, e2);
		cout << e3->evaluate() << endl;
		//------------------------------------------------------------------------------
		Expression* n32 = new Number(32.0);
		Expression* n16 = new Number(16.0);
		Expression* minus = new BinaryOperation(n32, BinaryOperation::MINUS, n16);
		Expression* callSqrt = new FunctionCall("sqrt", minus);
		Expression* n2 = new Number(2.0);
		Expression* mult = new BinaryOperation(n2, BinaryOperation::MUL, callSqrt);
		Expression* callAbs = new FunctionCall("abs", mult);
		cout << callAbs->evaluate() << endl;
		//------------------------------------------------------------------------------
	*/
	Number* n32 = new Number(32.0);
	Number* n16 = new Number(16.0);
	BinaryOperation* minus = new BinaryOperation(n32, BinaryOperation::MINUS, n16);
	FunctionCall* callSqrt = new FunctionCall("sqrt", minus);
	Variable* var = new Variable("var");
	BinaryOperation* mult = new BinaryOperation(var, BinaryOperation::MUL, callSqrt);
	FunctionCall* callAbs = new FunctionCall("abs", mult);
	CopySyntaxTree CST;
	Expression* newExpr = callAbs->transform(&CST);
	std::cout << newExpr->print() << std::endl;
	FoldConstants FC;
	Expression* newExpr2 = callAbs->transform(&FC);
	std::cout << newExpr2->print() << std::endl;
}