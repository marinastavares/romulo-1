 Array2=csvread('temposobrecarga_1.csv');
 Array=csvread('tempo.csv');
 t1=Array(:,1);
 t = Array2(:,1);
 hold on
 %histogram(10^(-4).*t1)
 histogram(10^(-4).*t)
 hold off