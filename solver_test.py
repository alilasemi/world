import numpy as np

# Dormand, J.R.; Prince, P.J. (1980). "A family of embedded Runge-Kutta formulae".
# Journal of Computational and Applied Mathematics.

aij = np.array([
    [0, 0, 0, 0, 0, 0],
    [1/5, 0, 0, 0, 0, 0],
    [3/40, 9/40, 0, 0, 0, 0],
    [44/45, -56/15, 32/9, 0, 0, 0],
    [19372/6561, -25360/2187, 64448/6561, -212/729, 0, 0],
    [9017/3168, -355/33, 46732/5247, 49/176, -5103/18656, 0],
    [35/384, 0, 500/1113, 125/192, -2187/6784, 11/84]
])
ci = np.sum(aij, axis=1)
bhat = np.array([35/384, 0, 500/1113, 125/192, -2187/6784, 11/84, 0])
b = np.array([5179/57600, 0, 7571/16695, 393/640, -92097/339200, 187/2100, 1/40])

k = np.empty(7)

def k_i(i, dt, x, t):
    x_new = x
    for j in range(i):
        x_new += aij[i, j] * dt * k[j]
    return f(x_new, t + ci[i] * dt)

def step(dt, x, t, first_step):
    if first_step:
        start_i = 0
    else:
        k[0] = k[-1]
        start_i = 1

    for i in range(start_i, 7):
        k[i] = k_i(i, dt, x, t)

    xhat = x
    for i in range(7):
        xhat += bhat[i] * dt * k[i]

    xnew = x
    for i in range(7):
        xnew += b[i] * dt * k[i]

    return xhat, xnew


def f(x, t):
    return x * np.cos(t)

x0 = 1.
t0 = 0.
t_final = 20

x = [x0]
t = [t0]
dt = [1e-5]
delta = 1e-3
first_step = True
while t[-1] < t_final:
    xhat, xnew = step(dt[-1], x[-1], t[-1], first_step)
    breakpoint()
    first_step = False

    error = np.abs(xnew - xhat)
    p = 4
#    dt_new = .9 * dt[-1] * (delta / np.abs(error)) ** (1/(p+1))
    dt_new = .02

    t.append(t[-1] + dt[-1])
    x.append(xnew)
    dt.append(dt_new)

print(dt)

# Plot
import matplotlib.pyplot as plt
t = np.array(t)
x = np.array(x)
t_exact = np.linspace(t0, t_final, 1000)
x_exact = np.exp(np.sin(t_exact))
plt.plot(t, x, label='Dormand-Prince')
plt.plot(t_exact, x_exact, label='Exact')
plt.xlabel('Time')
plt.ylabel('x(t)')
plt.legend()
plt.show()
