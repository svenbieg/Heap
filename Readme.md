<h1>Heap</h1>

<p>
This memory-manager is based on my <a href="http://www.github.com/svenbieg/clusters">Clusters</a> sorting-algorithm.<br />
Free space is mapped by size and by offset, so the smallest free block top most of the heap is returned.<br />
</p><br />

<img src="https://github.com/svenbieg/Heap/assets/12587394/c09b244b-989c-4cfa-96dd-5843c279c75c" /><br />
<br />

<p>
You can find detailed information in the <a href="https://github.com/svenbieg/Heap/wiki">Wiki</a>.
<br /><br />
This version is not for production-use, the offset-index is still not re-enterable. Also, the iteration has to be done twice if there is no gap of the needed size. This code is just a proof of concept, it will take me a few weeks to get it done.<br />
<br />
Best regards,<br />
<br />
Sven Bieg
</p><br />

<br /><br /><br /><br /><br />
