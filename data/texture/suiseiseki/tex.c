#include <stdio.h>

int main() {

	int n;

	printf("Insert: ");
	scanf("%i", &n);

	FILE *f = fopen("a.tex", "w");



	fprintf(f, "setshader bumpspecmapworld\n");
	fprintf(f, "\n");

	fprintf(f, "    texture c \"suiseiseki/%i.jpg\"\n", n);
	fprintf(f, "    texture n \"suiseiseki/%i.jpg\"\n", n + 1);
	fprintf(f, "    texture s \"suiseiseki/%i.jpg\"\n", n + 2);

	return 0;

}
