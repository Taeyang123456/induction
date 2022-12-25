#ifndef KINDUCTION_EXPR_HPP
#define KINDUCTION_EXPR_HPP



#include <set>
#include <vector>
#include <list>
#include <map>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

class Monomial{
public:
	Monomial(const double);
	Monomial(const llvm::Instruction*);
	Monomial(const double,const llvm::Instruction*);
	inline bool isConstant() const {
		return m_inst == nullptr;
	}
	inline bool isZero() const {
		return m_coef == 0.0;
	}
	inline double getCoefficient() const{
		return m_coef;
	}
	inline const llvm::Instruction* getInstruction() const{
		return m_inst;
	}
	Monomial getOppositeTerm() const {
		return Monomial(-m_coef,m_inst);
	}
	Monomial& operator+=(const Monomial&);
	Monomial& operator-=(const Monomial&);
	Monomial& operator*=(const double);
    Monomial& operator/=(const double);

private:
	double m_coef;
	const llvm::Instruction* m_inst;
};


class Polynomial{
	struct TermComparator{
		bool operator() (const Monomial& t0, const Monomial& t1) const{
			return t0.getInstruction() < t1.getInstruction();
		}
	};
public:
	Polynomial():m_CTerm(0.0){}
	Polynomial(const Monomial& term):m_CTerm(0.0){
		if(term.isConstant()){
			m_CTerm = term.getCoefficient();
		}
		else{
			m_terms.insert(term);
		}
	}
	inline const std::set<Monomial,TermComparator>& getMonomialsRef() const{
		return m_terms;
	}
	inline double getConstantTerm() const{
		return m_CTerm;
	}
	inline unsigned getNumVariables() const {
		return m_terms.size();
	}
	double getCoefficient(const llvm::Instruction* inst) const;
	double getCoefficientOrZero(const llvm::Instruction* inst) const;
	bool contains(const llvm::Instruction* inst) const;
	Polynomial& operator+=(const Monomial& RHS);
	Polynomial& operator-=(const Monomial& RHS);
	Polynomial& operator+=(const Polynomial& RHS);
	Polynomial& operator-=(const Polynomial& RHS);
	Polynomial& operator*=(const double RHS);
	Polynomial& operator/=(const double RHS);
	Polynomial operator+(const Monomial& RHS) const;
	Polynomial operator-(const Monomial& RHS) const;	
	Polynomial operator+(const Polynomial& RHS) const;
	Polynomial operator-(const Polynomial& RHS) const;
	Polynomial operator*(const double RHS) const;
	Polynomial operator/(const double) const;

private:
	double m_CTerm;
	std::set<Monomial,TermComparator> m_terms;
};


#endif
