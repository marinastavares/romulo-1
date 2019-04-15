 Array=csvread('dados-mv.csv');
 Array2=csvread('dados.csv');
 col1 = Array(:,1);
 col2 = Array2(:,1);
 histogram(10^(-3).*col1);
 histogram(10^(-3).*col2);
 