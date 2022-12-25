#include "kinduction/expr.hpp"
#include <cassert>

using namespace std;
using namespace llvm;

Monomial& Monomial::operator+=(const Monomial& RHS){
    if(this->isZero()){
        *this = RHS;
    }
    else if(!RHS.isZero()){
        assert(m_inst == RHS.m_inst);
        m_coef += RHS.m_coef;
        if(0.0 == m_coef){
            m_inst = nullptr;
        }
    }
    return *this;
}

Monomial& Monomial::operator-=(const Monomial& RHS){
	*this += RHS.getOppositeTerm();
	return *this;
}

Monomial& Monomial::operator*=(const double multiplier){
    m_coef *= multiplier;
	if(0.0 == multiplier){
        m_inst = nullptr;
    }
	return *this;
}

Monomial& Monomial::operator/=(const double divisor){
    assert(divisor);
    m_coef /= divisor;
	return *this;
}
double Polynomial::getCoefficient(const Instruction* inst) const{
	auto it =m_terms.find(inst);
	assert(it!=m_terms.end());
	return it->getCoefficient();
}

double Polynomial::getCoefficientOrZero(const Instruction* inst) const{
	auto it = m_terms.find(inst);
	if(it!=m_terms.end()){
		return it->getCoefficient();
	}
	else {
		return 0.0;
	}
}

bool Polynomial::contains(const Instruction* inst) const{
	return m_terms.count(inst);
}

Polynomial& Polynomial::operator+=(const Monomial& RHS){
	if(RHS.isConstant()){
		m_CTerm += RHS.getCoefficient();
	}
	else{
		auto res = m_terms.insert(RHS);
		if(!res.second){
			auto mono =  *res.first;
			mono+= RHS;
			m_terms.erase(res.first);
			if(!mono.isZero()){
				m_terms.insert(mono);
			}
		}
	}
	return *this;
}
Polynomial& Polynomial::operator-=(const Monomial& RHS){
	return *this+= RHS.getOppositeTerm();
}	
Polynomial& Polynomial::operator+=(const Polynomial& RHS){
	m_CTerm += RHS.getConstantTerm();
	for(auto term : RHS.m_terms){
		*this += term;
	}
	return *this;
}

Polynomial& Polynomial::operator-=(const Polynomial& RHS){
	m_CTerm -= RHS.getConstantTerm();
	for(auto term : RHS.m_terms){
		*this -= term;
	}
	return *this;
}

Polynomial& Polynomial::operator*=(const double RHS){
	if(0.0 == RHS){
		m_CTerm = 0.0;
		m_terms.clear();
	}
	else{
		m_CTerm *= RHS;
		set<Monomial,TermComparator> terms;
		for(auto t : m_terms){
			terms.insert(t*=RHS);
		}
		m_terms = terms;
	}
	return *this;
}

Polynomial& Polynomial::operator/=(const double RHS){
	assert(RHS);
	m_CTerm /= RHS;
	set<Monomial,TermComparator> terms;
	for(auto t : m_terms){
		terms.insert(t/=RHS);
	}
	m_terms=terms;
	return *this;
}

Polynomial Polynomial::operator+(const Monomial& RHS) const{
	Polynomial res(*this);
	return res+=RHS;
}

Polynomial Polynomial::operator-(const Monomial& RHS) const{
	Polynomial res(*this);
	return res-=RHS;
}	

Polynomial Polynomial::operator+(const Polynomial& RHS) const{
	Polynomial res(*this);
	return res+=RHS;
}

Polynomial Polynomial::operator-(const Polynomial& RHS) const{
	Polynomial res(*this);
	return res-=RHS;
}

Polynomial Polynomial::operator*(const double RHS) const{
	Polynomial res(*this);
	return res*=RHS;
}

Polynomial Polynomial::operator/(const double RHS) const{
	Polynomial res(*this);
	return res/=RHS;
}



